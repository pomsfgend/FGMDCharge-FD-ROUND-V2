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
    /* BHM: max voltage capability placeholder */
    uint16_t vmax = CUBE_DEFAULT_MAX_VOLTAGE_0P1V;
    d[0] = (uint8_t)(vmax & 0xFF);
    d[1] = (uint8_t)(vmax >> 8);
    (void)gbt_can_send(0x182756F4u, d, 2);
}

static void tx_bro_aa(void)
{
    uint8_t d[8] = {0xAA, 0, 0, 0, 0, 0, 0, 0};
    (void)gbt_can_send(0x100956F4u, d, 1);
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
    s_sub = 0;
    app_state_set_substate(0);
    app_state_set_session(true);
    set_state(ST_HANDSHAKE_INIT);
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

    switch (s_main) {
    case ST_HANDSHAKE_INIT:
        set_state(ST_WAIT_CRM);
        break;

    case ST_WAIT_CRM:
        if ((t - s_last_bhm_ms) >= 200) {
            tx_bhm();
            s_last_bhm_ms = t;
        }
        /* After CRM-like activity, move to param */
        if (s_crm_ok && (t - s_entry_ms) > 500) {
            set_state(ST_PARAM_CONFIG);
        }
        /* timeout 30 s */
        if ((t - s_entry_ms) > 30000) {
            ESP_LOGW(TAG, "WAIT_CRM timeout");
            set_state(ST_STOP);
            set_state(ST_IDLE);
        }
        break;

    case ST_PARAM_CONFIG:
        /* BCP long transport simplified: wait for CML then go READY */
        if (st.last_cml_ms != 0 && (t - st.last_cml_ms) < 5000) {
            set_state(ST_READY_WAIT_CRO);
        }
        if ((t - s_entry_ms) > 20000) {
            ESP_LOGW(TAG, "PARAM timeout");
            set_state(ST_STOP);
            set_state(ST_IDLE);
        }
        break;

    case ST_READY_WAIT_CRO:
        /* keep BRO AA periodic */
        if ((t - s_entry_ms) % 250 < CUBE_SM_TIMEBASE_MS) {
            tx_bro_aa();
        }
        if (s_cro_ok) {
            set_state(ST_CHARGING);
        }
        if ((t - s_entry_ms) > 30000) {
            ESP_LOGW(TAG, "CRO timeout");
            set_state(ST_STOP);
            set_state(ST_IDLE);
        }
        break;

    case ST_CHARGING:
        /* 10 s handoff when substate==3 */
        if (!s_handoff && s_sub == 3) {
            if ((t - s_entry_ms) >= CUBE_RELAY_HANDOFF_MS) {
                relays_set(0, 1); /* main power */
                s_handoff = true;
                app_state_set_handoff(true);
                ESP_LOGW(TAG, "RELAY HANDOFF: A=0 B=1 after %u ms", (unsigned)CUBE_RELAY_HANDOFF_MS);
            }
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
}
