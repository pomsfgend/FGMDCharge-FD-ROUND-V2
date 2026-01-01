#include "app_state.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "config.h"

static SemaphoreHandle_t s_lock;
static cube_state_snapshot_t s_state;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void lock_state(void)
{
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock_state(void)
{
    if (s_lock) xSemaphoreGive(s_lock);
}

void app_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));
    s_state.tx_enabled = CUBE_TX_DEFAULT_ENABLED != 0;
    s_state.main_state = ST_IDLE;
    s_state.bcl_requested_voltage_raw = CUBE_DEFAULT_REQ_VOLTAGE_0P1V;
    s_state.bcl_requested_current_0p1a = CUBE_DEFAULT_REQ_CURRENT_0P1A;
    s_state.bcl_mode = 0x01; /* constant current preferred */
    s_state.battery_voltage_0p1v = 0;
    s_state.soc_percent = 0;
}

void app_state_set_main_state(uint8_t s)
{
    lock_state();
    s_state.main_state = s;
    s_state.state_entry_ms = now_ms();
    s_state.handoff_done = false;
    if (s == ST_IDLE || s == ST_STOP || s == ST_FINALIZE) {
        s_state.session_active = false;
    }
    unlock_state();
}

void app_state_set_substate(uint8_t s)
{
    lock_state();
    s_state.substate = s;
    unlock_state();
}

void app_state_set_relays(uint8_t a, uint8_t b)
{
    lock_state();
    s_state.relay_a = a ? 1 : 0;
    s_state.relay_b = b ? 1 : 0;
    unlock_state();
}

void app_state_set_handoff(bool done)
{
    lock_state();
    s_state.handoff_done = done;
    unlock_state();
}

void app_state_set_session(bool active)
{
    lock_state();
    s_state.session_active = active;
    if (active) s_state.session_start_ms = now_ms();
    unlock_state();
}

void app_state_set_cml(uint16_t max_v, uint16_t min_v, int16_t max_i_0p1a, int16_t min_i_0p1a)
{
    lock_state();
    s_state.cml_max_voltage_raw = max_v;
    s_state.cml_min_voltage_raw = min_v;
    s_state.cml_max_current_0p1a = max_i_0p1a;
    s_state.cml_min_current_0p1a = min_i_0p1a;
    s_state.last_cml_ms = now_ms();
    s_state.charger_stage = CUBE_STAGE_PARAMETER;
    s_state.charger_stage_ms = s_state.last_cml_ms;
    s_state.last_charger_rx_ms = s_state.last_cml_ms;
    unlock_state();
}

void app_state_set_ccs(uint16_t out_v, int16_t out_i_0p1a, uint16_t charge_time_min, uint8_t permit_raw)
{
    lock_state();
    s_state.ccs_output_voltage_raw = out_v;
    s_state.ccs_output_current_0p1a = out_i_0p1a;
    s_state.ccs_charge_time_min = charge_time_min;
    s_state.ccs_charge_permit = (uint8_t)(permit_raw & 0x03U);
    s_state.ccs_status_raw = permit_raw;
    s_state.last_ccs_ms = now_ms();
    s_state.last_charger_rx_ms = s_state.last_ccs_ms;
    if ((permit_raw & 0x03U) == 0x01U) {
        s_state.charger_stage = CUBE_STAGE_CHARGING;
    } else if ((permit_raw & 0x03U) == 0x00U) {
        s_state.charger_stage = CUBE_STAGE_SUSPENDED;
    }
    s_state.charger_stage_ms = s_state.last_ccs_ms;
    unlock_state();
}

static char s_log[20][96];
static int s_log_n;
static float s_energy_wh;
static float s_cap_wh = 7500.0f;
static uint8_t s_soc_base;
static char s_vin[18] = "UNKNOWNVIN000000";

void app_state_log(const char *msg)
{
    if (!msg) return;
    int i = s_log_n % 20;
    snprintf(s_log[i], sizeof(s_log[i]), "%lu %s", (unsigned long)now_ms(), msg);
    s_log_n++;
}

int app_state_log_text(char *out, int out_len)
{
    if (!out || out_len < 8) return 0;
    int o = 0;
    int start = s_log_n > 20 ? s_log_n - 20 : 0;
    for (int n = start; n < s_log_n && o < out_len - 2; ++n) {
        o += snprintf(out + o, (size_t)(out_len - o), "%s\n", s_log[n % 20]);
    }
    if (o == 0) o = snprintf(out, (size_t)out_len, "no events\n");
    return o;
}

void app_state_set_vin(const char *vin)
{
    if (!vin) return;
    char tmp[18];
    int j = 0;
    for (int i = 0; vin[i] && j < 17; ++i) {
        unsigned char c = (unsigned char)vin[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) tmp[j++] = (char)c;
    }
    tmp[j] = 0;
    snprintf(s_vin, sizeof(s_vin), "%s", tmp[0] ? tmp : "UNKNOWNVIN000000");
}

const char *app_state_vin(void) { return s_vin; }

void app_state_set_bcl(uint16_t requested_v, int16_t requested_i_0p1a, uint8_t mode)
{
    lock_state();
    s_state.bcl_requested_voltage_raw = requested_v;
    s_state.bcl_requested_current_0p1a = requested_i_0p1a;
    s_state.bcl_mode = mode;
    unlock_state();
}

void app_state_set_cro(uint8_t status)
{
    lock_state();
    s_state.cro_status_raw = status;
    s_state.last_cro_ms = now_ms();
    s_state.last_charger_rx_ms = s_state.last_cro_ms;
    s_state.charger_stage = (status == 0xAAU) ? CUBE_STAGE_READY : CUBE_STAGE_PARAMETER;
    s_state.charger_stage_ms = s_state.last_cro_ms;
    unlock_state();
}

void app_state_set_cst(uint8_t status)
{
    lock_state();
    s_state.cst_status_raw = status;
    s_state.last_cst_ms = now_ms();
    s_state.last_charger_rx_ms = s_state.last_cst_ms;
    s_state.charger_stage = CUBE_STAGE_SUSPENDED;
    s_state.charger_stage_ms = s_state.last_cst_ms;
    unlock_state();
}

void app_state_set_csd(uint16_t time_min, uint16_t energy_0p1kwh)
{
    lock_state();
    s_state.csd_time_min = time_min;
    s_state.csd_energy_0p1kwh = energy_0p1kwh;
    s_state.last_csd_ms = now_ms();
    s_state.last_charger_rx_ms = s_state.last_csd_ms;
    s_state.charger_stage = CUBE_STAGE_COMPLETED;
    s_state.charger_stage_ms = s_state.last_csd_ms;
    unlock_state();
}

void app_state_set_cem(const uint8_t *data, uint8_t len)
{
    if (!data) return;
    if (len > 4U) len = 4U;
    lock_state();
    memset(s_state.cem_data, 0, sizeof(s_state.cem_data));
    memcpy(s_state.cem_data, data, len);
    s_state.last_cem_ms = now_ms();
    s_state.last_charger_rx_ms = s_state.last_cem_ms;
    s_state.charger_stage = CUBE_STAGE_ERROR;
    s_state.charger_stage_ms = s_state.last_cem_ms;
    unlock_state();
}

void app_state_set_charger_stage(cube_charge_stage_t stage)
{
    lock_state();
    s_state.charger_stage = stage;
    s_state.charger_stage_ms = now_ms();
    unlock_state();
}

void app_state_note_charger_rx(void)
{
    lock_state();
    s_state.last_charger_rx_ms = now_ms();
    unlock_state();
}

void app_state_set_can(bool up)
{
    lock_state();
    s_state.can_up = up;
    unlock_state();
}

void app_state_set_tx_enabled(bool enabled)
{
    lock_state();
    s_state.tx_enabled = enabled;
    unlock_state();
}

void app_state_set_ble(bool connected)
{
    lock_state();
    s_state.ble_connected = connected;
    unlock_state();
}

void app_state_set_ble_cccd(bool notify, bool indicate)
{
    lock_state();
    s_state.ble_notify_enabled = notify;
    s_state.ble_indicate_enabled = indicate;
    unlock_state();
}

void app_state_set_display(bool ready)
{
    lock_state();
    s_state.display_ready = ready;
    unlock_state();
}

void app_state_set_touch(bool enabled, bool pressed)
{
    lock_state();
    s_state.touch_enabled = enabled;
    s_state.touch_pressed = pressed;
    unlock_state();
}

void app_state_set_ble_rx(const uint8_t *data, uint16_t len)
{
    if (!data) return;
    lock_state();
    if (len > sizeof(s_state.last_ble_data)) len = sizeof(s_state.last_ble_data);
    memcpy(s_state.last_ble_data, data, len);
    s_state.last_ble_len = len;
    s_state.last_ble_rx_ms = now_ms();
    unlock_state();
}

void app_state_set_battery_vi(uint16_t v_0p1v, int16_t i_0p1a)
{
    lock_state();
    s_state.battery_voltage_0p1v = v_0p1v;
    s_state.battery_current_0p1a = i_0p1a;
    unlock_state();
}

void app_state_set_soc(uint8_t soc)
{
    s_soc_base = soc > 100 ? 100 : soc;
    float est = (float)s_soc_base + (s_cap_wh > 1.0f ? (s_energy_wh / s_cap_wh) * 100.0f : 0);
    if (est > 100.0f) est = 100.0f;
    lock_state();
    s_state.soc_percent = (uint8_t)est;
    unlock_state();
}

void app_state_set_cap_kwh(float kwh)
{
    if (kwh < 1.0f) kwh = 1.0f;
    if (kwh > 200.0f) kwh = 200.0f;
    s_cap_wh = kwh * 1000.0f;
    app_state_set_soc(s_soc_base);
}

float app_state_cap_kwh(void) { return s_cap_wh / 1000.0f; }

void app_state_set_protocol(uint8_t protocol)
{
    if (protocol > 3) protocol = 0;
    lock_state();
    s_state.protocol = protocol;
    unlock_state();
    app_state_log(protocol == 1 ? "protocol GBT-2015/A" :
                  protocol == 2 ? "protocol GBT-2011" :
                  protocol == 3 ? "protocol GBT-2023-B" : "protocol AUTO");
}

void app_state_add_energy(float watts, float dt_s)
{
    if (watts < 1 || dt_s <= 0) return;
    s_energy_wh += watts * dt_s / 3600.0f;
    float soc = (float)s_soc_base + (s_energy_wh / s_cap_wh) * 100.0f;
    if (soc > 100) soc = 100;
    lock_state();
    s_state.soc_percent = (uint8_t)soc;
    s_state.csd_energy_0p1kwh = (uint16_t)(s_energy_wh / 100.0f);
    unlock_state();
}

float app_state_energy_kwh(void) { return s_energy_wh / 1000.0f; }


const char *cube_main_state_name(uint8_t s)
{
    switch (s) {
        case ST_IDLE: return "IDLE";
        case ST_HANDSHAKE_INIT: return "HANDSHAKE_INIT";
        case ST_WAIT_CRM: return "WAIT_CRM";
        case ST_PARAM_CONFIG: return "PARAM_CONFIG";
        case ST_READY_WAIT_CRO: return "READY_WAIT_CRO";
        case ST_CHARGING: return "CHARGING";
        case ST_STOP: return "STOP";
        case ST_FINALIZE: return "FINALIZE";
        default: return "UNKNOWN";
    }
}

const char *cube_charge_stage_name(cube_charge_stage_t stage)
{
    switch (stage) {
        case CUBE_STAGE_HANDSHAKE: return "HANDSHAKE";
        case CUBE_STAGE_PARAMETER: return "PARAMETER";
        case CUBE_STAGE_READY: return "READY";
        case CUBE_STAGE_CHARGING: return "CHARGING";
        case CUBE_STAGE_SUSPENDED: return "SUSPENDED";
        case CUBE_STAGE_COMPLETED: return "COMPLETED";
        case CUBE_STAGE_ERROR: return "ERROR";
        default: return "NO DATA";
    }
}

void app_state_housekeeping(void)
{
    lock_state();
    s_state.uptime_ms = now_ms();
    if (s_state.last_charger_rx_ms != 0U) {
        uint32_t now = now_ms();
        if ((uint32_t)(now - s_state.last_charger_rx_ms) > 5000U &&
            s_state.charger_stage != CUBE_STAGE_COMPLETED &&
            s_state.charger_stage != CUBE_STAGE_ERROR) {
            s_state.charger_stage = CUBE_STAGE_NO_DATA;
            s_state.charger_stage_ms = now;
        }
    }
    unlock_state();
}

void app_state_snapshot(cube_state_snapshot_t *out)
{
    if (!out) return;
    lock_state();
    *out = s_state;
    unlock_state();
}

const char *cube_protocol_name(uint8_t protocol)
{
    if (protocol == 1) return "GBT-2015/A";
    if (protocol == 2) return "GBT-2011";
    if (protocol == 3) return "GBT-2023-B";
    return "AUTO";
}

void app_state_can_rx(const cube_can_frame_t *f, bool charger_frame)
{
    int proto_log = 0;
    if (!f) return;
    lock_state();
    s_state.last = *f;
    s_state.rx_count++;
    if (charger_frame) s_state.last_charger_rx_ms = now_ms();
    /* PF|PS|SA, priority ignored.
       0x0038255F is GB/T 27930-2023 B version frame (SECC 0x5F -> EVCC 0x25).
       CHM 26F4 is 2015 and 2023 class A (same V1.1 messages).
       CRM 01F4 with no CHM and no B frame is 2011. */
    if ((f->id & 0x00FFFFFFu) == 0x0038255Fu) {
        if (s_state.protocol != 3) proto_log = 3;
        s_state.protocol = 3;
    } else if ((f->id & 0x00FFFF00u) == 0x0026F400u && s_state.protocol != 3) {
        if (s_state.protocol != 1) proto_log = 1;
        s_state.protocol = 1;
    } else if ((f->id & 0x00FFFF00u) == 0x0001F400u) {
        if (s_state.protocol == 0) {
            s_state.protocol = 2;
            proto_log = 2;
        }
        if (f->dlc > 0) {
            s_state.crm_status = f->data[0];
            s_state.last_crm_ms = now_ms();
        }
    }
    unlock_state();
    if (proto_log == 1) app_state_log("CHM -> GBT-2015/2023-A");
    else if (proto_log == 2) app_state_log("CRM without CHM -> GBT-2011");
    else if (proto_log == 3) app_state_log("version frame -> GBT-2023-B");
    if (charger_frame) {
        static uint32_t last_logged;
        if (f->id != last_logged) {
            char m[48];
            snprintf(m, sizeof(m), "RX id 0x%08lX dlc %u", (unsigned long)f->id, f->dlc);
            app_state_log(m);
            last_logged = f->id;
        }
    }
    if (f->id == 0x081FF456u) app_state_log("CEM charger error, session kept");
    if (f->id == 0x101AF456u) app_state_log("CST from charger, session kept");
}

void app_state_can_tx_ok(void)
{
    lock_state();
    s_state.tx_count++;
    unlock_state();
}

void app_state_can_tx_drop(void)
{
    /* counted as soft fail; no separate field required */
}
