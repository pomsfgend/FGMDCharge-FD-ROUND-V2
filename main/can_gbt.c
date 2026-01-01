#include "can_gbt.h"
#include <limits.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"
#include "gbt_tp.h"

static const char *TAG = "GBT_CAN";
static SemaphoreHandle_t s_lock;
static cube_can_frame_t s_history[CUBE_CAN_HISTORY];
static size_t s_head;
static size_t s_count;
static bool s_running;
static bool s_tx_enabled;
static gbt_can_diag_t s_diag;
static gbt_tp_rx_t s_tp_chg;
static gbt_tp_rx_t s_tp_bat;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t decode_current_0p1a(uint16_t raw)
{
    int32_t value = (int32_t)raw - 4000;
    if (value > INT16_MAX) value = INT16_MAX;
    if (value < INT16_MIN) value = INT16_MIN;
    return (int16_t)value;
}

static bool is_charger_id(uint32_t id)
{
    switch (id) {
        case GBT_ID_CTS:
        case GBT_ID_CRM:
        case GBT_ID_CHM:
        case GBT_ID_CRO:
        case GBT_ID_CST:
        case GBT_ID_CSD:
        case GBT_ID_CEM:
        case GBT_ID_CML:
        case GBT_ID_CCS:
        case GBT_ID_TP_CM_CHG:
        case GBT_ID_TP_DT_CHG:
            return true;
        default:
            return false;
    }
}

static void decode_known(const twai_message_t *rx)
{
    const uint8_t *d = rx->data;
    switch (rx->identifier) {
        case GBT_ID_CHM:
            app_state_set_charger_stage(CUBE_STAGE_HANDSHAKE);
            break;
        case GBT_ID_CRM:
            app_state_set_charger_stage(CUBE_STAGE_HANDSHAKE);
            break;
        case GBT_ID_CML:
            if (rx->data_length_code >= 8U) {
                /* GB/T/J1939 SPNs are little-endian on the CAN payload. */
                uint16_t max_v = le16(&d[0]);
                uint16_t min_v = le16(&d[2]);
                int16_t max_i = decode_current_0p1a(le16(&d[4]));
                int16_t min_i = decode_current_0p1a(le16(&d[6]));
                app_state_set_cml(max_v, min_v, max_i, min_i);
            }
            break;
        case GBT_ID_CRO:
            if (rx->data_length_code >= 1U) app_state_set_cro(d[0]);
            break;
        case GBT_ID_CCS:
            if (rx->data_length_code >= 7U) {
                uint16_t out_v = le16(&d[0]);
                int16_t out_i = decode_current_0p1a(le16(&d[2]));
                uint16_t charge_time = le16(&d[4]);
                uint8_t permit = d[6];
                app_state_set_ccs(out_v, out_i, charge_time, permit);
            }
            break;
        case GBT_ID_CST:
            if (rx->data_length_code >= 1U) app_state_set_cst(d[0]);
            break;
        case GBT_ID_CSD:
            if (rx->data_length_code >= 4U) {
                app_state_set_csd(le16(&d[0]), le16(&d[2]));
            }
            break;
        case GBT_ID_CEM:
            if (rx->data_length_code >= 1U) app_state_set_cem(d, rx->data_length_code);
            break;
        case GBT_ID_BCL:
            if (rx->data_length_code >= 5U) {
                app_state_set_bcl(le16(&d[0]), decode_current_0p1a(le16(&d[2])), d[4]);
            }
            break;
        default:
            break;
    }
}

static void update_bus_state(void)
{
    if (!s_running) {
        app_state_set_can(false);
        return;
    }

    twai_status_info_t info = {0};
    if (twai_get_status_info(&info) != ESP_OK) return;

    if (info.state == TWAI_STATE_BUS_OFF) {
        app_state_set_can(false);
        esp_err_t err = twai_initiate_recovery();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "TWAI recovery start failed: %s", esp_err_to_name(err));
        }
    } else if (info.state == TWAI_STATE_STOPPED) {
        esp_err_t err = twai_start();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            app_state_set_can(false);
            ESP_LOGW(TAG, "TWAI restart after recovery failed: %s", esp_err_to_name(err));
        } else {
            app_state_set_can(true);
        }
    } else {
        app_state_set_can(info.state == TWAI_STATE_RUNNING);
    }
}

esp_err_t gbt_can_init(void)
{
    if (s_running) return ESP_OK;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CUBE_CAN_TX_GPIO, CUBE_CAN_RX_GPIO, TWAI_MODE_NORMAL);
    g.tx_queue_len = CUBE_CAN_TX_QUEUE;
    g.rx_queue_len = CUBE_CAN_RX_QUEUE;
    twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err != ESP_OK) return err;
    err = twai_start();
    if (err != ESP_OK) {
        (void)twai_driver_uninstall();
        return err;
    }

    s_running = true;
    s_tx_enabled = CUBE_TX_DEFAULT_ENABLED != 0;
    memset(s_history, 0, sizeof(s_history));
    memset(&s_diag, 0, sizeof(s_diag));
    s_head = 0;
    s_count = 0;
    gbt_tp_init(&s_tp_chg);
    gbt_tp_init(&s_tp_bat);
    app_state_set_can(true);
    app_state_set_tx_enabled(s_tx_enabled);
    ESP_LOGI(TAG, "CAN/TWAI up: TX=%d RX=%d bitrate=%d", CUBE_CAN_TX_GPIO, CUBE_CAN_RX_GPIO, CUBE_CAN_BITRATE);
    return ESP_OK;
}

void gbt_can_set_tx_enabled(bool enabled)
{
#if CUBE_TX_COMPILED_ENABLE
    if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_tx_enabled = enabled;
    if (s_lock) (void)xSemaphoreGive(s_lock);
    app_state_set_tx_enabled(enabled);
#else
    (void)enabled;
    if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_tx_enabled = false;
    if (s_lock) (void)xSemaphoreGive(s_lock);
    app_state_set_tx_enabled(false);
#endif
}

bool gbt_can_get_tx_enabled(void)
{
    bool enabled = false;
    if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    enabled = s_tx_enabled;
    if (s_lock) (void)xSemaphoreGive(s_lock);
    return enabled;
}

esp_err_t gbt_can_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    if (!s_running || !data || dlc > 8U || id > 0x1FFFFFFFu) return ESP_ERR_INVALID_ARG;

    bool tx_enabled = false;
    if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    tx_enabled = s_tx_enabled;
    if (s_lock) (void)xSemaphoreGive(s_lock);
    if (!tx_enabled) {
        app_state_can_tx_drop();
        return ESP_ERR_INVALID_STATE;
    }

    twai_message_t tx = {0};
    tx.identifier = id;
    tx.data_length_code = dlc;
    if (id > 0x7FFu) tx.flags = TWAI_MSG_FLAG_EXTD;
    memcpy(tx.data, data, dlc);

    esp_err_t err = twai_transmit(&tx, pdMS_TO_TICKS(100));
    if (err == ESP_OK) app_state_can_tx_ok();
    else app_state_can_tx_drop();
    if (err == ESP_ERR_INVALID_STATE) update_bus_state();
    return err;
}

void gbt_can_task(void *arg)
{
    (void)arg;
    twai_message_t rx;

    for (;;) {
        if (!s_running) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        memset(&rx, 0, sizeof(rx));
        esp_err_t err = twai_receive(&rx, pdMS_TO_TICKS(1000));
        if (err == ESP_OK) {
            cube_can_frame_t f = {0};
            f.id = rx.identifier;
            f.dlc = rx.data_length_code > 8U ? 8U : rx.data_length_code;
            f.flags = rx.flags;
            f.timestamp_ms = now_ms();
            memcpy(f.data, rx.data, f.dlc);

            if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
            s_history[s_head] = f;
            s_head = (s_head + 1U) % CUBE_CAN_HISTORY;
            if (s_count < CUBE_CAN_HISTORY) s_count++;
            s_diag.rx_frames++;
            s_diag.last_id = f.id;
            s_diag.last_dlc = f.dlc;
            memcpy(s_diag.last_data, f.data, sizeof(s_diag.last_data));
            if (s_lock) (void)xSemaphoreGive(s_lock);

            app_state_can_rx(&f, is_charger_id(f.id));
            decode_known(&rx);

            gbt_tp_rx_t *tp = NULL;
            if (f.id == GBT_ID_TP_CM_CHG || f.id == GBT_ID_TP_DT_CHG) {
                tp = &s_tp_chg;
            } else if (f.id == GBT_ID_TP_CM_BAT || f.id == GBT_ID_TP_DT_BAT) {
                tp = &s_tp_bat;
            }

            if (tp) {
                const uint8_t *payload = NULL;
                size_t payload_len = 0;
                if (gbt_tp_consume(tp, f.id, f.data, f.dlc, &payload, &payload_len)) {
                    ESP_LOGI(TAG, "GB/T TP payload complete: %u bytes", (unsigned)payload_len);
                    if (payload_len >= 4U && f.id == GBT_ID_TP_DT_BAT) {
                        /* BCP/BCS/etc. are retained as raw TP payload until their exact
                           original dispatch mapping is fully proven from the dump. */
                    }
                    (void)payload;
                }
            }
            app_state_set_can(true);
        } else {
            update_bus_state();
        }
    }
}

esp_err_t gbt_can_get_last(cube_can_frame_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    *out = st.last;
    return ESP_OK;
}

esp_err_t gbt_can_get_history(cube_can_frame_t *out, size_t max_count, size_t *actual_count)
{
    if (!out || !actual_count || max_count == 0U) return ESP_ERR_INVALID_ARG;
    if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = s_count < max_count ? s_count : max_count;
    size_t start = (s_head + CUBE_CAN_HISTORY - n) % CUBE_CAN_HISTORY;
    for (size_t i = 0; i < n; ++i) {
        out[i] = s_history[(start + i) % CUBE_CAN_HISTORY];
    }
    if (s_lock) (void)xSemaphoreGive(s_lock);
    *actual_count = n;
    return ESP_OK;
}

void gbt_can_get_diag(gbt_can_diag_t *out)
{
    if (!out) return;
    if (s_lock) (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_diag;
    if (s_lock) (void)xSemaphoreGive(s_lock);
}
