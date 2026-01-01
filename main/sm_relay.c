/*
 * State machine + relay control reconstructed from original FGMDCharge dump.
 * Sequence (CONFIRMED):
 *   State4 entry  -> GPIO6 = HIGH (prepare, first click of the relay test)
 *   State5 + CCS >= 50.0V stable 3s -> GPIO6 = LOW, GPIO7 = HIGH (load, second click)
 *   Stop / idle / error -> both relays LOW
 * BLE 0x66 only touches GPIO6 + substate, never GPIO7.
 */
#include "sm_relay.h"
#include "app_state.h"
#include "config.h"
#include "can_gbt.h"
#include "gbt_b.h"
#include "session_log.h"
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
static bool     s_boot_started = false;
static uint32_t s_power_seen_ms = 0;
static bool s_load_on = false;
static uint16_t s_v_anchor = 0;
/* -1: load follows station voltage. 0/1: long-hold latched the load relay until the opposite hold. */
static int s_load_manual = -1;
static uint32_t s_last_opt_ms = 0;

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

    if (s == ST_IDLE || s == ST_STOP || s == ST_FINALIZE ||
        s == ST_HANDSHAKE_INIT || s == ST_CHARGING) {
        if (s_load_manual < 0) {
            s_load_on = false;
            s_power_seen_ms = 0;
            s_v_anchor = 0;
        }
    }
    if (s == ST_IDLE || s == ST_STOP || s == ST_FINALIZE) {
        relays_fail_safe();
        app_state_set_session(false);
        if (s_load_manual == 1) {
            relays_set(0, 1);
            s_load_on = true;
        }
    }
    if (s == ST_READY_WAIT_CRO) {
        s_sub = 3;
        app_state_set_substate(3);
        relays_set(1, s_load_manual == 1 ? 1 : 0); /* prepare path, keep a latched load */
    }
    ESP_LOGI(TAG, "state -> %s (%u) sub=%u", cube_main_state_name(s), (unsigned)s, (unsigned)s_sub);
}

static void end_session(void)
{
    session_log_request(1, SESSION_LOG_STOP);
    app_state_release_auto_protocol();
    if (s_main == ST_IDLE) return;
    set_state(ST_STOP);
    set_state(ST_FINALIZE);
    set_state(ST_IDLE);
}

/* --- TX helpers (only when TX enabled) --- */
static bool st_proto_is_2011(void);

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
    /* 2011 piles expect J1939 priority 6 on BRM/BCP/BCS. 2015 and 2023-A use 7. */
    const int old = st_proto_is_2011();
    const uint32_t cm_id = old ? 0x18EC56F4u : 0x1CEC56F4u;
    const uint32_t dt_id = old ? 0x18EB56F4u : 0x1CEB56F4u;
    uint8_t packets = (uint8_t)((len + 6) / 7);
    uint8_t cm[8] = {
        32, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8), packets, 0xFF,
        (uint8_t)(pgn & 0xFF), (uint8_t)((pgn >> 8) & 0xFF), (uint8_t)((pgn >> 16) & 0xFF)
    };
    (void)gbt_can_send(cm_id, cm, 8);
    for (uint8_t seq = 1; seq <= packets; ++seq) {
        uint8_t dt[8];
        memset(dt, 0xFF, sizeof(dt));
        dt[0] = seq;
        uint16_t off = (uint16_t)(seq - 1) * 7;
        for (int i = 0; i < 7 && (off + i) < len; ++i) dt[1 + i] = payload[off + i];
        (void)gbt_can_send(dt_id, dt, 8);
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
    b[0] = st_proto_is_2011() ? 0 : 1; /* 2011 is V1.0 -> 00 01 00, 2015 is V1.1 -> 01 01 00 */
    b[1] = 1;
    b[2] = 0;
    b[3] = 6;                     /* ternary lithium */
    b[4] = 0xE8; b[5] = 0x03;     /* 100.0 Ah */
    b[6] = 0xA0; b[7] = 0x0F;     /* 400.0 V */
    b[23] = 0xFF;                 /* reserved bits filled with 1 */
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
    int32_t i_abs = st.bcl_requested_current_0p1a;
    int32_t imax;
    uint16_t energy = (uint16_t)(app_state_cap_kwh() * 10.0f); /* 0.1 kWh */
    uint16_t vmax = CUBE_DEFAULT_MAX_VOLTAGE_0P1V;
    uint16_t vnow = st.bcl_requested_voltage_raw ? st.bcl_requested_voltage_raw : 2400;
    uint16_t soc_01;
    if (i_abs < 0) i_abs = -i_abs;
    if (st.last_cml_ms != 0 && st.cml_max_current_0p1a < 0) {
        int32_t lim = -(int32_t)st.cml_max_current_0p1a;
        if (i_abs > lim) i_abs = lim;
    }
    if (vnow > vmax) vmax = vnow;
    /* Charge current is negative. raw = 4000 - |I|. +I is outside -400..0 A and the pile aborts. */
    imax = 4000 - i_abs;
    if (imax < 0) imax = 0;
    if (imax > 65535) imax = 65535;
    soc_01 = (uint16_t)(st.soc_percent ? st.soc_percent : 50) * 10u;
    b[0] = (uint8_t)(cell & 0xFF); b[1] = (uint8_t)(cell >> 8);
    b[2] = (uint8_t)(imax & 0xFF); b[3] = (uint8_t)(imax >> 8);
    b[4] = (uint8_t)(energy & 0xFF); b[5] = (uint8_t)(energy >> 8);
    b[6] = (uint8_t)(vmax & 0xFF); b[7] = (uint8_t)(vmax >> 8);
    b[8] = 90; /* 40 C, offset -50 */
    b[9] = (uint8_t)(soc_01 & 0xFF);
    b[10] = (uint8_t)(soc_01 >> 8);          /* SOC is 0.1 % in 2015 and 2023-A */
    b[11] = (uint8_t)(vnow & 0xFF);
    b[12] = (uint8_t)(vnow >> 8);
    tx_bam(0x000600u, b, 13);
    app_state_log("TX BCP 13B");
}

static void tx_bro(uint8_t ready)
{
    uint8_t d[8] = {ready, 0, 0, 0, 0, 0, 0, 0};
    (void)gbt_can_send(0x100956F4u, d, 1);
}

/* BEM bits: 00 normal, 01 timeout, 10 untrusted. One 2-bit field per missed message. */
static void tx_bem(uint8_t which)
{
    /* Reserved bits are 1. Each 2-bit field is 01 only for the missed message. */
    uint8_t d[4] = {0xF0, 0xF0, 0xF0, 0xFC};
    switch (which) {
        case 0: d[0] = 0xF1; break; /* CRM = 0x00 */
        case 1: d[0] = 0xF4; break; /* CRM = 0xAA */
        case 2: d[1] = 0xF1; break; /* CML */
        case 3: d[1] = 0xF4; break; /* CRO */
        case 4: d[2] = 0xF1; break; /* CCS */
        case 5: d[2] = 0xF4; break; /* CST */
        default: d[3] = 0xFD; break; /* CSD */
    }
    (void)gbt_can_send(GBT_ID_BEM, d, 4);
    app_state_log("TX BEM timeout, relays off");
}

static void tx_bsd(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint8_t d[8] = {0};
    uint16_t cell_min = 330; /* 3.30 V, 0.01 V */
    uint16_t cell_max = 420; /* 4.20 V */
    d[0] = st.soc_percent;
    d[1] = (uint8_t)(cell_min & 0xFF);
    d[2] = (uint8_t)(cell_min >> 8);
    d[3] = (uint8_t)(cell_max & 0xFF);
    d[4] = (uint8_t)(cell_max >> 8);
    d[5] = 70; /* 20 C, offset -50 */
    d[6] = 80; /* 30 C */
    (void)gbt_can_send(GBT_ID_BSD, d, 7);
    app_state_log("TX BSD end");
}

static void tx_bcl(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    uint8_t d[8] = {0};
    uint16_t v = st.bcl_requested_voltage_raw;
    int32_t i_abs = st.bcl_requested_current_0p1a;
    int32_t i_enc;
    if (i_abs < 0) i_abs = -i_abs;
    if (st.last_cml_ms != 0 && st.cml_max_current_0p1a < 0) {
        int32_t lim = -(int32_t)st.cml_max_current_0p1a;
        if (i_abs > lim) i_abs = lim;
    }
    i_enc = 4000 - i_abs;
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
    uint8_t b[9];
    uint16_t v = st.battery_voltage_0p1v ? st.battery_voltage_0p1v : st.ccs_output_voltage_raw;
    int32_t i_abs = st.battery_current_0p1a ? st.battery_current_0p1a : st.ccs_output_current_0p1a;
    uint16_t cell = 420; /* 4.20 V, group 0 */
    int32_t i_enc;
    if (i_abs < 0) i_abs = -i_abs;
    i_enc = 4000 - i_abs;
    if (i_enc < 0) i_enc = 0;
    if (i_enc > 65535) i_enc = 65535;
    memset(b, 0, sizeof(b));
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(i_enc & 0xFF);
    b[3] = (uint8_t)(i_enc >> 8);
    b[4] = (uint8_t)(cell & 0xFF);
    b[5] = (uint8_t)(cell >> 8);
    b[6] = st.soc_percent;
    b[7] = 0x58; /* 600 min. Zero means "finished" and a lot of piles stop. */
    b[8] = 0x02;
    tx_bam(0x001100u, b, 9);
}

static void tx_bsm(void)
{
    uint16_t bits = 0;
    uint8_t d[8];
    memset(d, 0, sizeof(d));
    d[0] = 1;   /* highest-cell index, numbering starts at 1 */
    d[1] = 75;  /* 25 C, offset -50. Zero would be -50 C and trips the pile. */
    d[2] = 1;
    d[3] = 70;  /* 20 C */
    d[4] = 1;
    bits |= (1u << 12); /* charge allowed */
    bits |= (3u << 14); /* reserved bits filled with 1 */
    d[5] = (uint8_t)(bits & 0xFF);
    d[6] = (uint8_t)(bits >> 8);
    (void)gbt_can_send(GBT_ID_BSM, d, 7);
}

/* Optional in every GB/T 27930 edition. 2011 period is 1 s, 2015/2023-A is 10 s. */
static void tx_optional_cells(void)
{
    uint8_t mv[2];
    uint8_t mt[1];
    uint8_t sp[1];
    uint16_t cell = 420; /* 4.20 V in 12 bits, group 0 */
    mv[0] = (uint8_t)(cell & 0xFF);
    mv[1] = (uint8_t)(cell >> 8);
    mt[0] = 75;
    sp[0] = 0xFF;
    tx_bam(0x001500u, mv, 2);
    tx_bam(0x001600u, mt, 1);
    tx_bam(0x001700u, sp, 1);
    app_state_log("TX BMV BMT BSP");
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
    gbt_b_reset();
    s_sub = 0;
    app_state_set_substate(0);
    app_state_set_session(true);
    set_state(ST_HANDSHAKE_INIT);
}

void sm_request_relay_test(void)
{
    cube_state_snapshot_t st;
    int nb;
    app_state_snapshot(&st);
    nb = st.relay_b ? 0 : 1;
    s_load_manual = nb;
    s_load_on = nb ? true : false;
    /* GPIO6 (prepare) stays as it is. GPIO7 is the load and latches. */
    relays_set(st.relay_a ? 1 : 0, nb);
    app_state_log(nb ? "нагрузка ВКЛ, держится" : "нагрузка ВЫКЛ, держится");
}

void sm_stop_session(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    if (st.protocol == 3) gbt_b_request_stop();
    else if (s_main == ST_CHARGING || s_main == ST_READY_WAIT_CRO ||
        s_main == ST_PARAM_CONFIG || s_main == ST_WAIT_CRM) {
        tx_bst();
    }
    end_session();
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
    if (st.rx_count != s_last_rx_count) {
        s_last_rx_count = st.rx_count;
        s_last_chg_ms = t;
    }

    bool skip_a = false;
    if (st.protocol == 3) {
        gbt_b_tick(t);
        if (gbt_b_stop()) {
            if (s_main != ST_IDLE) {
                app_state_log("B session end");
                gbt_b_reset();
                end_session();
            }
        } else if (gbt_b_energy()) {
            if (s_main != ST_CHARGING) set_state(ST_CHARGING);
        }
        if (s_main != ST_CHARGING) skip_a = true;
    }

    if (!skip_a) switch (s_main) {
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
        if (st.last_crm_ms != 0) {
            if (st.crm_status == 0xAA) s_crmaa = true;
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
            tx_bem(s_crm00 ? 1 : 0);
            end_session();
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
            tx_bem(2);
            end_session();
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
            tx_bem(3);
            end_session();
        }
        break;

    case ST_CHARGING:
        /* GPIO6 is prepare. GPIO7 is the load relay and stays under the manual latch if the user held it. */
        if (s_load_manual >= 0) {
            if ((int)st.relay_b != s_load_manual) {
                relays_set(st.relay_a ? 1 : 0, s_load_manual);
            }
        } else if (!s_load_on) {
            uint16_t v = st.ccs_output_voltage_raw;
            if (v >= 500) {
                if (s_power_seen_ms == 0 || s_v_anchor == 0) {
                    s_power_seen_ms = t;
                    s_v_anchor = v;
                } else {
                    uint32_t lo = (uint32_t)s_v_anchor - (uint32_t)s_v_anchor / 5u;
                    uint32_t hi = (uint32_t)s_v_anchor + (uint32_t)s_v_anchor / 5u;
                    if (v < lo || v > hi) {
                        s_power_seen_ms = t;
                        s_v_anchor = v;
                    }
                }
                if ((t - s_power_seen_ms) >= 3000) {
                    relays_set(0, 1);
                    s_load_on = true;
                    s_handoff = true;
                    app_state_set_handoff(true);
                    app_state_log("нагрузка ВКЛ, напряжение стабильно");
                }
            } else {
                s_power_seen_ms = 0;
                s_v_anchor = 0;
            }
        } else if (s_load_manual < 0 && s_load_on && st.ccs_output_voltage_raw < 300) {
            relays_set(0, 0);
            s_load_on = false;
            s_power_seen_ms = 0;
            s_v_anchor = 0;
            app_state_log("нагрузка ВЫКЛ, напряжение пропало");
        }
        {
            float v = st.ccs_output_voltage_raw / 10.0f;
            float i = st.ccs_output_current_0p1a / 10.0f;
            if (i < 0) i = -i;
            if (v > 5 && i > 0.2f) app_state_add_energy(v * i, 0.01f);
        }
        if (st.protocol != 3) {
            /* periodic BCL / BCS / BSM and optional BMV/BMT/BSP */
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
            {
                uint32_t opt_ms = st_proto_is_2011() ? 1000u : 10000u;
                if ((t - s_last_opt_ms) >= opt_ms) {
                    tx_optional_cells();
                    s_last_opt_ms = t;
                }
            }
            if (st.charger_stage == CUBE_STAGE_COMPLETED ||
                st.charger_stage == CUBE_STAGE_ERROR ||
                st.charger_stage == CUBE_STAGE_SUSPENDED) {
                tx_bst();
                tx_bsd();
                end_session();
            }
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
