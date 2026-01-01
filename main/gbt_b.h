#pragma once

#include <stdbool.h>
#include <stdint.h>

/* GB/T 27930-2023 category B (ChaoJi) on the same 250 kbit/s port.
   SECC 0x5F, EVCC 0x25. Version frame PF=0x38. Transport PF=0x34..0x37. */

void gbt_b_reset(void);
void gbt_b_on_rx(uint32_t id, const uint8_t *data, uint8_t dlc);
void gbt_b_tick(uint32_t now_ms);
void gbt_b_request_stop(void);
bool gbt_b_energy(void);
bool gbt_b_stop(void);
