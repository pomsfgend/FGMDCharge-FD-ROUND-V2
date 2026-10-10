#include "gbt_tp.h"
#include <string.h>
#include "can_gbt.h"

void gbt_tp_init(gbt_tp_rx_t *ctx) { memset(ctx, 0, sizeof(*ctx)); }

bool gbt_tp_consume(gbt_tp_rx_t *ctx, uint32_t can_id, const uint8_t *data, uint8_t dlc,
                    const uint8_t **payload, size_t *payload_len) {
    if (!ctx || !data || !payload || !payload_len || dlc > 8) return false;
    *payload = NULL; *payload_len = 0;

    if (can_id != GBT_ID_TP_CM_CHG && can_id != GBT_ID_TP_DT_CHG &&
        can_id != GBT_ID_TP_CM_BAT && can_id != GBT_ID_TP_DT_BAT) return false;

    if (can_id == GBT_ID_TP_CM_CHG || can_id == GBT_ID_TP_CM_BAT) {
        /* GB/T/J1939 TP.CM BAM-like start: byte0 0x20 for BAM, 0x10 for RTS. */
        if (dlc < 5) return false;
        if (data[0] != 0x20 && data[0] != 0x10) return false;
        uint16_t total = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
        if (total == 0 || total > GBT_TP_MAX_PAYLOAD) { memset(ctx, 0, sizeof(*ctx)); return false; }
        ctx->active = true;
        ctx->packet_type = data[0];
        ctx->total_len = total;
        ctx->received = 0;
        ctx->next_seq = 1;
        return false;
    }

    if (!ctx->active || dlc < 2) return false;
    uint8_t seq = data[0];
    if (seq != ctx->next_seq) { memset(ctx, 0, sizeof(*ctx)); return false; }
    size_t copy = dlc - 1;
    if ((size_t)ctx->received + copy > sizeof(ctx->data)) { memset(ctx, 0, sizeof(*ctx)); return false; }
    if ((size_t)ctx->received + copy > ctx->total_len) copy = ctx->total_len - ctx->received;
    memcpy(&ctx->data[ctx->received], &data[1], copy);
    ctx->received += (uint16_t)copy;
    ctx->next_seq++;
    if (ctx->received >= ctx->total_len) {
        *payload = ctx->data;
        *payload_len = ctx->total_len;
        ctx->active = false;
        return true;
    }
    return false;
}
