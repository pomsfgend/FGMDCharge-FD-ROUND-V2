#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/twai.h"
#include "app_state.h"

/* Charger -> BMS IDs recovered from the original cube firmware. */
#define GBT_ID_CTS          0x1807F456u
#define GBT_ID_CRM          0x1801F456u
#define GBT_ID_CHM          0x1826F456u
#define GBT_ID_CRO          0x100AF456u
#define GBT_ID_CST          0x101AF456u
#define GBT_ID_CSD          0x181DF456u
#define GBT_ID_CEM          0x081FF456u
#define GBT_ID_CML          0x1808F456u
#define GBT_ID_CCS          0x1812F456u

/* BMS -> charger IDs retained for passive observation. */
#define GBT_ID_BHM          0x182756F4u
#define GBT_ID_BRO          0x100956F4u
#define GBT_ID_BCL          0x181056F4u
#define GBT_ID_BCS          0x181156F4u
#define GBT_ID_BSM          0x181356F4u
#define GBT_ID_BST          0x101956F4u
#define GBT_ID_BSD          0x181C56F4u
#define GBT_ID_BEM          0x081E56F4u
#define GBT_ID_BCP          0x180656F4u

#define GBT_ID_TP_CM_BAT    0x1CEC56F4u
#define GBT_ID_TP_DT_BAT    0x1CEB56F4u
#define GBT_ID_TP_CM_CHG    0x1CECF456u
#define GBT_ID_TP_DT_CHG    0x1CEBF456u

typedef struct {
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t last_id;
    uint8_t last_dlc;
    uint8_t last_data[8];
} gbt_can_diag_t;

esp_err_t gbt_can_init(void);
esp_err_t gbt_can_send(uint32_t id, const uint8_t *data, uint8_t dlc);
void gbt_can_set_tx_enabled(bool enabled);
bool gbt_can_get_tx_enabled(void);
esp_err_t gbt_can_get_last(cube_can_frame_t *out);
esp_err_t gbt_can_get_history(cube_can_frame_t *out, size_t max_count, size_t *actual_count);
void gbt_can_task(void *arg);
void gbt_can_get_diag(gbt_can_diag_t *out);
