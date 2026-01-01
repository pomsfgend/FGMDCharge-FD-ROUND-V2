#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t flags;
    uint32_t timestamp_ms;
} cube_can_frame_t;

/* GB/T battery-side main states (matches original 0..7) */
typedef enum {
    ST_IDLE           = 0,
    ST_HANDSHAKE_INIT = 1,
    ST_WAIT_CRM       = 2,
    ST_PARAM_CONFIG   = 3,
    ST_READY_WAIT_CRO = 4,
    ST_CHARGING       = 5,
    ST_STOP           = 6,
    ST_FINALIZE       = 7
} cube_main_state_t;

typedef enum {
    CUBE_STAGE_NO_DATA = 0,
    CUBE_STAGE_HANDSHAKE,
    CUBE_STAGE_PARAMETER,
    CUBE_STAGE_READY,
    CUBE_STAGE_CHARGING,
    CUBE_STAGE_SUSPENDED,
    CUBE_STAGE_COMPLETED,
    CUBE_STAGE_ERROR
} cube_charge_stage_t;

typedef struct {
    uint8_t  main_state;
    uint8_t  substate;
    bool     handoff_done;
    uint32_t state_entry_ms;
    uint8_t  relay_a;
    uint8_t  relay_b;

    cube_charge_stage_t charger_stage;
    uint32_t charger_stage_ms;
    uint32_t last_charger_rx_ms;

    uint16_t cml_max_voltage_raw;
    uint16_t cml_min_voltage_raw;
    int16_t  cml_max_current_0p1a;
    int16_t  cml_min_current_0p1a;
    uint32_t last_cml_ms;

    uint16_t ccs_output_voltage_raw;
    int16_t  ccs_output_current_0p1a;
    uint16_t ccs_charge_time_min;
    uint8_t  ccs_charge_permit;
    uint8_t  ccs_status_raw;
    uint32_t last_ccs_ms;

    uint8_t  cro_status_raw;
    uint32_t last_cro_ms;
    uint8_t  cst_status_raw;
    uint32_t last_cst_ms;
    uint16_t csd_time_min;
    uint16_t csd_energy_0p1kwh;
    uint32_t last_csd_ms;
    uint8_t  cem_data[4];
    uint32_t last_cem_ms;

    uint16_t bcl_requested_voltage_raw;
    int16_t  bcl_requested_current_0p1a;
    uint8_t  bcl_mode;

    uint16_t battery_voltage_0p1v;
    int16_t  battery_current_0p1a;
    uint8_t  soc_percent;

    bool can_up;
    bool tx_enabled;
    bool ble_connected;
    bool ble_notify_enabled;
    bool ble_indicate_enabled;
    bool display_ready;
    bool touch_enabled;
    bool touch_pressed;

    uint8_t  last_ble_data[64];
    uint16_t last_ble_len;
    uint32_t last_ble_rx_ms;

    bool     session_active;
    uint32_t session_start_ms;
    uint32_t uptime_ms;

    cube_can_frame_t last;
    uint32_t rx_count;
    uint32_t tx_count;
} cube_state_snapshot_t;

void app_state_init(void);
void app_state_housekeeping(void);
void app_state_snapshot(cube_state_snapshot_t *out);

void app_state_set_main_state(uint8_t s);
void app_state_set_substate(uint8_t s);
void app_state_set_relays(uint8_t a, uint8_t b);
void app_state_set_handoff(bool done);
void app_state_set_session(bool active);

void app_state_set_cml(uint16_t max_v, uint16_t min_v, int16_t max_i_0p1a, int16_t min_i_0p1a);
void app_state_set_ccs(uint16_t out_v, int16_t out_i_0p1a, uint16_t charge_time_min, uint8_t permit_raw);
void app_state_set_bcl(uint16_t requested_v, int16_t requested_i_0p1a, uint8_t mode);
void app_state_set_cro(uint8_t status);
void app_state_set_cst(uint8_t status);
void app_state_set_csd(uint16_t time_min, uint16_t energy_0p1kwh);
void app_state_set_cem(const uint8_t *data, uint8_t len);
void app_state_set_charger_stage(cube_charge_stage_t stage);
void app_state_note_charger_rx(void);

void app_state_set_can(bool up);
void app_state_set_tx_enabled(bool enabled);
void app_state_set_ble(bool connected);
void app_state_set_ble_cccd(bool notify, bool indicate);
void app_state_set_display(bool ready);
void app_state_set_touch(bool enabled, bool pressed);
void app_state_set_ble_rx(const uint8_t *data, uint16_t len);
void app_state_set_battery_vi(uint16_t v_0p1v, int16_t i_0p1a);
void app_state_set_soc(uint8_t soc);

const char *cube_main_state_name(uint8_t s);
void app_state_can_rx(const cube_can_frame_t *f, bool charger_frame);
void app_state_can_tx_ok(void);
void app_state_can_tx_drop(void);

const char *cube_charge_stage_name(cube_charge_stage_t stage);
