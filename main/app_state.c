#include "app_state.h"
#include <string.h>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "config.h"

static cube_state_snapshot_t s_state;
static SemaphoreHandle_t s_lock;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void app_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));
    s_state.tx_enabled = CUBE_TX_DEFAULT_ENABLED != 0;
    s_state.cml_max_voltage_raw = 0;
    s_state.cml_min_voltage_raw = 0;
    s_state.cml_max_current_0p1a = INT16_MIN;
    s_state.cml_min_current_0p1a = INT16_MIN;
    s_state.ccs_output_current_0p1a = INT16_MIN;
    s_state.ccs_charge_permit = 0xFF;
    s_state.charger_stage = CUBE_STAGE_NO_DATA;
}

static void lock_state(void)
{
    if (s_lock) {
        (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock_state(void)
{
    if (s_lock) {
        (void)xSemaphoreGive(s_lock);
    }
}

void app_state_can_rx(const cube_can_frame_t *f, bool charger_frame)
{
    if (!f) return;
    lock_state();
    s_state.last = *f;
    s_state.rx_count++;
    if (charger_frame) {
        s_state.last_charger_rx_ms = f->timestamp_ms;
    }
    unlock_state();
}

void app_state_can_tx_ok(void)
{
    lock_state();
    s_state.tx_count++;
    unlock_state();
}

void app_state_can_tx_drop(void)
{
    lock_state();
    s_state.dropped_count++;
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
    if ((permit_raw & 0x03U) == 0x01U) {
        s_state.charger_stage = CUBE_STAGE_CHARGING;
    } else if ((permit_raw & 0x03U) == 0x00U) {
        s_state.charger_stage = CUBE_STAGE_SUSPENDED;
    }
    s_state.charger_stage_ms = s_state.last_ccs_ms;
    unlock_state();
}

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
    s_state.charger_stage = (status == 0xAAU) ? CUBE_STAGE_READY : CUBE_STAGE_PARAMETER;
    s_state.charger_stage_ms = s_state.last_cro_ms;
    unlock_state();
}

void app_state_set_cst(uint8_t status)
{
    lock_state();
    s_state.cst_status_raw = status;
    s_state.last_cst_ms = now_ms();
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
