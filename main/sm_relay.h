#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t sm_relay_init(void);
void sm_relay_tick(void);          /* call every 10 ms */
void sm_start_session(void);       /* begin charge sequence from IDLE */
void sm_stop_session(void);
void sm_request_relay_test(void); /* long-hold: latch GPIO7 load only, leave GPIO6 */
void sm_ble_cmd_0x66(uint8_t val); /* original BLE relay override on GPIO6 only */
void sm_ble_cmd_0x0D(void);        /* substate=6 */
uint8_t sm_get_main_state(void);
uint8_t sm_get_substate(void);
bool sm_get_handoff_done(void);
