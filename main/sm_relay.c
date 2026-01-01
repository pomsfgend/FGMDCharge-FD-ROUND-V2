/*
 * State machine + relay control reconstructed from original FGMDCharge dump.
 * Sequence (CONFIRMED):
 *   State4 entry  -> GPIO6 = HIGH (prepare)
 *   State5 + 10s + substate==3 -> GPIO6 = LOW, GPIO7 = HIGH (main power)
 *   Stop / idle / error -> both relays LOW
 * BLE 0x66 only touches GPIO6 + substate, never GPIO7.
 */
#include "sm_relay.h"
#include "app_state.h"
#include "config.h"
#include "can_gbt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SM_RELAY";

static uint8_t  s_main = ST_IDLE;
static uint8_t  s_sub  = 0;
static bool     s_handoff = false;
static uint32_t s_entry_ms = 0;
static bool     s_crm_ok = false;
static bool     s_cro_ok = false;
static uint32_t s_last_bhm_ms = 0;
static uint32_t s_last_bcl_ms = 0;
static uint32_t s_last_bcs_ms = 0;
static uint32_t s_last_bsm_ms = 0;
static bool     s_crm00 = false;
static bool     s_crmaa = false;
static uint32_t s_last_brm_ms = 0;
static uint32_t s_last_bcp_ms = 0;
static uint32_t s_last_rx_count = 0;
static uint32_t s_last_chg_ms = 0;
static uint32_t s_auto_hold_until = 0;
static uint8_t  s_relay_test = 0;
static uint32_t s_relay_test_ms = 0;
static bool     s_boot_started = false;
static uint32_t s_power_seen_ms = 0;
static bool s_load_on = false;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void relays_set(int a, int b)
{
    gpio_set_level(CUBE_RELAY_A_GPIO, a ? 1 : 0);
    gpio_set_level(CUBE_RELAY_B_GPIO, b ? 1 : 0);
    app_state_set_relays((uint8_t)(a ? 1 : 0), (uint8_t)(b ? 1 : 0));
}

static void relays_fail_safe(void)
{
    relays_set(0, 0);
}

static void set_state(uint8_t s)
{
    s_main = s;
    s_entry_ms = now_ms();
    s_handoff = false;
    app_state_set_main_state(s);
    app_state_set_handoff(false);

    if (s == ST_IDLE || s == ST_STOP || s == ST_FINALIZE) {
        relays_fail_safe();
        app_state_set_session(false);
    }
    if (s == ST_READY_WAIT_CRO) {
        s_sub = 3;
        app_state_set_substate(3);
        relays_set(1, 0); /* prepare path */
    }
    ESP_LOGI(TAG, "state -> %s (%u) sub=%u", cube_main_state_name(s), (unsigned)s, (unsigned)s_sub);
}

/* --- TX helpers (only when TX enabled) --- */
static void tx_bhm(void)
{
    uint8_t d[8] = {0};
    uint16_t vmax = CUBE_DEFAULT_MAX_VOLTAGE_0P1V;
    d[0] = (uint8_t)(vmax & 0xFF);
    d[1] = (uint8_t)(vmax >> 8);
    (void)gbt_can_send(0x182756F4u, d, 2);
}

/* J1939 BAM: stations drop the session if BRM/BCP are not multi-frame. */
static void tx_bam(uint32_t pgn, const uint8_t *payload, uint16_t len)
{
    uint8_t packets = (uint8_t)((len + 6) / 7);
    uint8_t cm[8] = {
        32, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8), packets, 0xFF,
        (uint8_t)(pgn & 0xFF), (uint8_t)((pgn >> 8) & 0xFF), (uint8_t)((pgn >> 16) & 0xFF)
    };
    (void)gbt_can_send(0x1CEC56F4u, cm, 8);
    for (uint8_t seq = 1; seq <= packets; ++seq) {
        uint8_t dt[8];
        memset(dt, 0xFF, sizeof(dt));
        dt[0] = seq;
        uint16_t off = (uint16_t)(seq - 1) * 7;
        for (int i = 0; i < 7 && (off + i) < len; ++i) dt[1 + i] = payload[off + i];
        (void)gbt_can_send(0x1CEB56F4u, dt, 8);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


static bool st_proto_is_2011(void)
{
    cube_state_snapshot_t snap;
    app_state_snapshot(&snap);
    return snap.protocol == 2;
}

static void tx_brm(void)
{
    uint8_t b[49];
    memset(b, 0, sizeof(b));
    b[0] = 1; b[1] = 1; b[2] = 0; /* protocol 1.1.0 */
    b[3] = 6;                     /* lithium */
    b[4] = 0xE8; b[5] = 0x03;     /* 100.0 Ah */
    b[6] = 0xA0; b[7] = 0x0F;     /* 400.0 V */
    memcpy(&b[24], "00000000000000000", 17);
    {
        const char *vin = app_state_vin();
        for (int i = 0; i < 17 && vin && vin[i]; ++i) b[24 + i] = (uint8_t)vin[i];
    }
    if (st_proto_is_2011()) {
        tx_bam(0x000200u, b, 41);
        app_state_log("TX BRM 41B GBT-2011");
    } else {
        tx_bam(0x000200u, b, 49);
        app_state_log("TX BRM 49B GBT-2015");
    }
}

static void tx_bcp(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint8_t b[13];
    memset(b, 0, sizeof(b));
    uint16_t cell = 420; /* 4.20 V */
    uint16_t imax = (uint16_t)(4000 - 2500); /* 250 A allowed, offset -400 A */
    uint16_t energy = (uint16_t)(app_state_cap_kwh() * 10.0f); /* 0.1 kWh */
    uint16_t vmax = CUBE_DEFAULT_MAX_VOLTAGE_0P1V;
    uint16_t vnow = st.bcl_requested_voltage_raw ? st.bcl_requested_voltage_raw : 2400;
    b[0] = (uint8_t)(cell & 0xFF); b[1] = (uint8_t)(cell >> 8);
    b[2] = (uint8_t)(imax & 0xFF); b[3] = (uint8_t)(imax >> 8);
    b[4] = (uint8_t)(energy & 0xFF); b[5] = (uint8_t)(energy >> 8);
    b[6] = (uint8_t)(vmax & 0xFF); b[7] = (uint8_t)(vmax >> 8);
    b[8] = 90; /* 40 C, offset -50 */
    b[9] = st.soc_percent ? st.soc_percent : 50;
    b[10] = (uint8_t)(vnow & 0xFF); b[11] = (uint8_t)(vnow >> 8);
    tx_bam(0x000600u, b, 13);
    app_state_log("TX BCP 13B");
}

static void tx_bro(uint8_t ready)
{
    uint8_t d[8] = {ready, 0, 0, 0, 0, 0, 0, 0};
    (void)gbt_can_send(0x100956F4u, d, 1);
}

static void tx_bem(uint8_t code)
{
    uint8_t d[8] = {code, 0, 0, 0, 0, 0, 0, 0};
    (void)gbt_can_send(0x081E56F4u, d, 4);
    app_state_log("TX BEM timeout, relays off");
}

static void tx_bsd(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint8_t d[8] = {st.soc_percent, 0, 0, 0, 0, 0, 0, 0};
    (void)gbt_can_send(0x181C56F4u, d, 7);
    app_state_log("TX BSD end");
}

static void tx_bcl(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint8_t d[8] = {0};
    uint16_t v = st.bcl_requested_voltage_raw;
    /* GB/T current encoding: value + 4000 in 0.1 A units */
    int32_t i_enc = (int32_t)st.bcl_requested_current_0p1a + 4000;
    if (i_enc < 0) i_enc = 0;
    if (i_enc > 65535) i_enc = 65535;
    d[0] = (uint8_t)(v & 0xFF);
    d[1] = (uint8_t)(v >> 8);
    d[2] = (uint8_t)(i_enc & 0xFF);
    d[3] = (uint8_t)(i_enc >> 8);
    d[4] = st.bcl_mode;
    (void)gbt_can_send(0x181056F4u, d, 5);
}

static void tx_bcs(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint8_t d[8] = {0};
    uint16_t v = st.battery_voltage_0p1v;
    int32_t i_enc = (int32_t)st.battery_current_0p1a + 4000;
    if (i_enc < 0) i_enc = 0;
    if (i_enc > 65535) i_enc = 65535;
    d[0] = (uint8_t)(v & 0xFF);
    d[1] = (uint8_t)(v >> 8);
    d[2] = (uint8_t)(i_enc & 0xFF);
    d[3] = (uint8_t)(i_enc >> 8);
    d[4] = 0x00; /* block voltage placeholder */
    d[5] = st.soc_percent;
    d[6] = 0x00; /* remaining time placeholder */
    d[7] = 0x00;
    (void)gbt_can_send(0x181156F4u, d, 8);
}

static void tx_bsm(void)
{
    uint8_t d[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    (void)gbt_can_send(0x181356F4u, d, 7);
}

static void tx_bst(void)
{
    uint8_t d[8] = {0x01, 0, 0, 0}; /* normal stop */
    (void)gbt_can_send(0x101956F4u, d, 4);
}

esp_err_t sm_relay_init(void)
{
    /* Drive LOW before switching to output to avoid relay coil glitch/brownout. */
    gpio_set_level(CUBE_RELAY_A_GPIO, 0);
    gpio_set_level(CUBE_RELAY_B_GPIO, 0);
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << CUBE_RELAY_A_GPIO) | (1ULL << CUBE_RELAY_B_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;
    gpio_set_level(CUBE_RELAY_A_GPIO, 0);
    gpio_set_level(CUBE_RELAY_B_GPIO, 0);
    s_main = ST_IDLE;
    s_sub = 0;
    s_handoff = false;
    s_entry_ms = now_ms();
    app_state_set_main_state(ST_IDLE);
    app_state_set_substate(0);
    app_state_set_handoff(false);
    app_state_set_relays(0, 0);
    app_state_set_session(false);
    ESP_LOGW(TAG, "relays OK A=GPIO%d B=GPIO%d both OFF", CUBE_RELAY_A_GPIO, CUBE_RELAY_B_GPIO);
    return ESP_OK;
}

void sm_start_session(void)
{
    if (s_main != ST_IDLE && s_main != ST_STOP && s_main != ST_FINALIZE) {
        ESP_LOGW(TAG, "start ignored, already in state %u", (unsigned)s_main);
        return;
    }
    s_crm_ok = false;
    s_cro_ok = false;
    s_crm00 = false;
    s_crmaa = false;
    s_sub = 0;
    app_state_set_substate(0);
    app_state_set_session(true);
    set_state(ST_HANDSHAKE_INIT);
}

void sm_request_relay_test(void)
{
    if (s_main == ST_CHARGING || s_relay_test) return;
    s_relay_test = 1;
    s_relay_test_ms = now_ms();
    relays_set(1, 0);
    app_state_log("relay test GPIO6 ON");
}

void sm_stop_session(void)
{
    if (s_main == ST_CHARGING || s_main == ST_READY_WAIT_CRO ||
        s_main == ST_PARAM_CONFIG || s_main == ST_WAIT_CRM) {
        tx_bst();
    }
    set_state(ST_STOP);
    /* short finalize then idle */
    set_state(ST_FINALIZE);
    set_state(ST_IDLE);
}

void sm_ble_cmd_0x66(uint8_t val)
{
    if (val == 0) {
        gpio_set_level(CUBE_RELAY_A_GPIO, 0);
        /* original: only GPIO6; leave B as-is */
        int b = gpio_get_level(CUBE_RELAY_B_GPIO);
        app_state_set_relays(0, (uint8_t)(b ? 1 : 0));
        s_sub = 6;
        app_state_set_substate(6);
        ESP_LOGI(TAG, "BLE 0x66=0 -> GPIO6 OFF substate=6");
    } else if (val == 1) {
        gpio_set_level(CUBE_RELAY_A_GPIO, 1);
        int b = gpio_get_level(CUBE_RELAY_B_GPIO);
        app_state_set_relays(1, (uint8_t)(b ? 1 : 0));
        s_sub = 3;
        app_state_set_substate(3);
        ESP_LOGI(TAG, "BLE 0x66=1 -> GPIO6 ON substate=3");
    }
}

void sm_ble_cmd_0x0D(void)
{
    s_sub = 6;
    app_state_set_substate(6);
    ESP_LOGI(TAG, "BLE 0x0D -> substate=6");
}

uint8_t sm_get_main_state(void) { return s_main; }
uint8_t sm_get_substate(void) { return s_sub; }
bool sm_get_handoff_done(void) { return s_handoff; }

/* Called every 10 ms from CAN/SM task */
void sm_relay_tick(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint32_t t = now_ms();

    /* Observe charger signals for transitions */
    if (st.cro_status_raw == 0xAA) s_cro_ok = true;
    if (st.charger_stage == CUBE_STAGE_HANDSHAKE || st.last_cml_ms != 0) {
        /* CRM presence approximated by any handshake traffic */
    }
    /* Simple CRM OK: we saw CHM/CRM stage */
    if (st.charger_stage == CUBE_STAGE_HANDSHAKE || st.charger_stage == CUBE_STAGE_PARAMETER) {
        s_crm_ok = true;
    }

    if (!s_boot_started && t > 400) {
        s_boot_started = true;
        app_state_log("12V up, handshake now");
        sm_start_session();
    }
    if (s_relay_test) {
        uint32_t dt = t - s_relay_test_ms;
        if (s_relay_test == 1 && dt >= 600) {
            relays_set(0, 1);
            s_relay_test = 2;
            app_state_log("relay test GPIO7 ON");
        } else if (s_relay_test == 2 && dt >= 1200) {
            relays_set(0, 0);
            s_relay_test = 0;
            app_state_log("relay test off");
        }
    }
    if (st.rx_count != s_last_rx_count) {
        s_last_rx_count = st.rx_count;
        s_last_chg_ms = t;
    }

    switch (s_main) {
    case ST_IDLE:
        if (t > s_auto_hold_until && s_last_chg_ms != 0 && (t - s_last_chg_ms) < 1500) {
            app_state_log("auto start: station on CAN");
            sm_start_session();
        }
        break;
    case ST_HANDSHAKE_INIT:
        set_state(ST_WAIT_CRM);
        break;

    case ST_WAIT_CRM:
        if (st.protocol == 1 && (t - s_last_bhm_ms) >= 250) {
            tx_bhm();
            s_last_bhm_ms = t;
        }
        if (st.last.id == 0x1801F456u && st.last.dlc > 0) {
            if (st.last.data[0] == 0xAA) s_crmaa = true;
            else s_crm00 = true;
        }
        if (s_crm00 && !s_crmaa && (t - s_last_brm_ms) >= 250) {
            tx_brm();
            s_last_brm_ms = t;
        }
        if (s_crmaa) {
            app_state_log("CRM AA, go BCP");
            set_state(ST_PARAM_CONFIG);
        }
        if ((t - s_entry_ms) > 30000) {
            tx_bem(0x01);
            set_state(ST_IDLE);
        }
        break;

    case ST_PARAM_CONFIG:
        if ((t - s_last_bcp_ms) >= 500) {
            tx_bcp();
            s_last_bcp_ms = t;
        }
        if (st.last_cml_ms != 0 && (t - st.last_cml_ms) < 5000) {
            app_state_log("CML ok, BRO AA");
            set_state(ST_READY_WAIT_CRO);
        }
        if ((t - s_entry_ms) > 20000) {
            tx_bem(0x04);
            set_state(ST_IDLE);
        }
        break;

    case ST_READY_WAIT_CRO:
        /* 2 s BRO=00: vehicle insulation / contactors not ready, then AA */
        if ((t - s_entry_ms) % 250 < CUBE_SM_TIMEBASE_MS) {
            if ((t - s_entry_ms) < 2000) tx_bro(0x00);
            else tx_bro(0xAA);
        }
        if (s_cro_ok) {
            set_state(ST_CHARGING);
        }
        if ((t - s_entry_ms) > 30000) {
            tx_bem(0x08);
            set_state(ST_IDLE);
        }
        break;

    case ST_CHARGING:
        /* 10 s handoff when substate==3 */
        /* GPIO6 onboard already on. GPIO7 external load 3 s after station voltage. */
        if (st.ccs_output_voltage_raw > 500) {
            if (s_power_seen_ms == 0) s_power_seen_ms = t;
            /* Current setpoint is the BCL request, not a trip. Keep asking. */
            if (!s_load_on && (t - s_power_seen_ms) >= 3000) {
                relays_set(1, 1);
                s_load_on = true;
                s_handoff = true;
                app_state_set_handoff(true);
                app_state_log("load GPIO7 ON, 3s after station power");
            }
        }
        {
            float v = st.ccs_output_voltage_raw / 10.0f;
            float i = st.ccs_output_current_0p1a / 10.0f;
            if (i < 0) i = -i;
            if (v > 5 && i > 0.2f) app_state_add_energy(v * i, 0.01f);
        }
        /* periodic BCL / BCS / BSM */
        if ((t - s_last_bcl_ms) >= 50) {
            tx_bcl();
            s_last_bcl_ms = t;
        }
        if ((t - s_last_bcs_ms) >= 250) {
            tx_bcs();
            s_last_bcs_ms = t;
        }
        if ((t - s_last_bsm_ms) >= 250) {
            tx_bsm();
            s_last_bsm_ms = t;
        }
        /* charger stop / error */
        if (st.charger_stage == CUBE_STAGE_COMPLETED ||
            st.charger_stage == CUBE_STAGE_ERROR ||
            st.charger_stage == CUBE_STAGE_SUSPENDED) {
            tx_bst();
            tx_bsd();
            set_state(ST_STOP);
            set_state(ST_FINALIZE);
            set_state(ST_IDLE);
        }
        break;

    case ST_STOP:
    case ST_FINALIZE:
        relays_fail_safe();
        break;

    default:
        break;
    }
    if (s_main != ST_IDLE && s_last_chg_ms != 0 && (t - s_last_chg_ms) > 3000) {
        app_state_log("unplug: CAN silent, relays off");
        s_auto_hold_until = t + 8000;
        sm_stop_session();
    }
}
