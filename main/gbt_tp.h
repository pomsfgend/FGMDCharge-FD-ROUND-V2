#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBT_TP_MAX_PAYLOAD 0x6F0

typedef struct {
    bool active;
    uint8_t packet_type;
    uint16_t total_len;
    uint16_t received;
    uint8_t next_seq;
    uint8_t data[GBT_TP_MAX_PAYLOAD];
} gbt_tp_rx_t;

void gbt_tp_init(gbt_tp_rx_t *ctx);
/* Returns true only when a complete payload was assembled. */
bool gbt_tp_consume(gbt_tp_rx_t *ctx, uint32_t can_id, const uint8_t *data, uint8_t dlc,
                    const uint8_t **payload, size_t *payload_len);
