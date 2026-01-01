#include "ble.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "app_state.h"
#include "config.h"
#include "settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if !defined(CONFIG_BT_BLUEDROID_ENABLED) || !CONFIG_BT_BLUEDROID_ENABLED
#error "FD_ROUND V2 BLE requires CONFIG_BT_BLUEDROID_ENABLED=y"
#endif

static const char *TAG = "BLE";
static uint16_t s_service_handle;
static uint16_t s_attr_handles[8];
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0xFFFF;
static bool s_notify;
static bool s_indicate;
static bool s_adv_ready;
static bool s_initialized;
static char s_ble_name[32] = CUBE_BLE_NAME;

/* GAP APIs consume these structures asynchronously, so keep them static. */
static esp_ble_adv_data_t s_adv_data;
static uint16_t s_primary_service_decl_uuid = ESP_GATT_UUID_PRI_SERVICE;
static uint16_t s_char1_uuid = CUBE_BLE_CHAR1_UUID;
static uint16_t s_char2_uuid = CUBE_BLE_CHAR2_UUID;
static uint16_t s_char3_uuid = CUBE_BLE_CHAR3_UUID;
static uint16_t s_char_decl_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static uint16_t s_cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static uint16_t s_service_uuid = CUBE_BLE_SERVICE_UUID;
static uint8_t s_char1_value[4] = {0x11, 0x22, 0x33, 0x44};
static uint8_t s_char2_value[4] = {0x11, 0x22, 0x33, 0x44};
static uint8_t s_char3_value[4] = {0x11, 0x22, 0x33, 0x44};
static uint8_t s_cccd[2] = {0, 0};
static const uint8_t s_char1_prop = ESP_GATT_CHAR_PROP_BIT_READ |
                                    ESP_GATT_CHAR_PROP_BIT_WRITE |
                                    ESP_GATT_CHAR_PROP_BIT_NOTIFY |
                                    ESP_GATT_CHAR_PROP_BIT_INDICATE;
static const uint8_t s_char_write_prop = ESP_GATT_CHAR_PROP_BIT_WRITE;

static const esp_gatts_attr_db_t gatt_db[8] = {
    [0] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_primary_service_decl_uuid,
            ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(s_service_uuid),
            (uint8_t *)&s_service_uuid}},
    [1] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_char_decl_uuid,
            ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(s_char1_prop),
            (uint8_t *)&s_char1_prop}},
    [2] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_char1_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 500, sizeof(s_char1_value),
            s_char1_value}},
    [3] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_cccd_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(s_cccd), sizeof(s_cccd),
            s_cccd}},
    [4] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_char_decl_uuid,
            ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(s_char_write_prop),
            (uint8_t *)&s_char_write_prop}},
    [5] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_char2_uuid,
            ESP_GATT_PERM_WRITE, 500, sizeof(s_char2_value),
            s_char2_value}},
    [6] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_char_decl_uuid,
            ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(s_char_write_prop),
            (uint8_t *)&s_char_write_prop}},
    [7] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_char3_uuid,
            ESP_GATT_PERM_WRITE, 500, sizeof(s_char3_value),
            s_char3_value}},
};

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void on_write(uint16_t handle, const uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;
    app_state_set_ble_rx(data, len);
    ESP_LOGI(TAG, "RX handle=%u len=%u", (unsigned)handle, (unsigned)len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, len > 64 ? 64 : len, ESP_LOG_INFO);
    /* The original BLE OTA framing is not proven. Arbitrary BLE writes never
       trigger firmware updates; web OTA is the controlled update path. */
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
            s_adv_ready = true;
            esp_err_t err = esp_ble_gap_start_advertising(&s_adv_params);
            if (err != ESP_OK) ESP_LOGE(TAG, "start advertising failed: %s", esp_err_to_name(err));
            break;
        }
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "advertising start failed status=%d", param->adv_start_cmpl.status);
            } else {
                ESP_LOGI(TAG, "advertising started");
            }
            break;
        default:
            break;
    }
}

static void gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                     esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            s_gatts_if = gatts_if;
            esp_err_t err = esp_ble_gap_set_device_name(s_ble_name);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "set BLE name failed: %s", esp_err_to_name(err));
                break;
            }

            memset(&s_adv_data, 0, sizeof(s_adv_data));
            s_adv_data.set_scan_rsp = false;
            s_adv_data.include_name = true;
            s_adv_data.include_txpower = true;
            s_adv_data.service_uuid_len = sizeof(s_service_uuid);
            s_adv_data.p_service_uuid = (uint8_t *)&s_service_uuid;
            s_adv_data.flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;

            err = esp_ble_gap_config_adv_data(&s_adv_data);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "config advertising data failed: %s", esp_err_to_name(err));
                break;
            }
            err = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 8, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "create GATT table request failed: %s", esp_err_to_name(err));
            }
            break;
        }
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == 8) {
                memcpy(s_attr_handles, param->add_attr_tab.handles, sizeof(s_attr_handles));
                s_service_handle = s_attr_handles[0];
                esp_err_t err = esp_ble_gatts_start_service(s_service_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "start GATT service failed: %s", esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG, "GATT service started, base handle=%u", (unsigned)s_service_handle);
                }
            } else {
                ESP_LOGE(TAG, "create attr table failed status=%d num=%d",
                         param->add_attr_tab.status, param->add_attr_tab.num_handle);
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            s_conn_id = param->connect.conn_id;
            app_state_set_ble(true);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            s_conn_id = 0xFFFF;
            s_notify = false;
            s_indicate = false;
            app_state_set_ble(false);
            app_state_set_ble_cccd(false, false);
            if (s_adv_ready) {
                esp_err_t err = esp_ble_gap_start_advertising(&s_adv_params);
                if (err != ESP_OK) ESP_LOGW(TAG, "restart advertising failed: %s", esp_err_to_name(err));
            }
            break;
        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_attr_handles[3] &&
                !param->write.is_prep && param->write.len == sizeof(s_cccd)) {
                uint16_t v = (uint16_t)param->write.value[0] |
                             ((uint16_t)param->write.value[1] << 8);
                s_notify = (v & 0x0001U) != 0;
                s_indicate = (v & 0x0002U) != 0;
                app_state_set_ble_cccd(s_notify, s_indicate);
                ESP_LOGI(TAG, "FFE1 CCCD notify=%d indicate=%d", s_notify, s_indicate);
            } else if (!param->write.is_prep) {
                on_write(param->write.handle, param->write.value, param->write.len);
            }
            break;
        default:
            break;
    }
}

esp_err_t cube_ble_init(void) {
    if (s_initialized) return ESP_OK;

    esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "release classic BT memory: %s", esp_err_to_name(err));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) return err;

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) goto fail_controller;

    (void)settings_get_ble_name(s_ble_name, sizeof(s_ble_name));

    err = esp_bluedroid_init();
    if (err != ESP_OK) goto fail_controller_enabled;

    err = esp_bluedroid_enable();
    if (err != ESP_OK) goto fail_bluedroid;

    err = esp_ble_gap_register_callback(gap_cb);
    if (err != ESP_OK) goto fail_bluedroid_enabled;

    err = esp_ble_gatts_register_callback(gatts_cb);
    if (err != ESP_OK) goto fail_bluedroid_enabled;

    err = esp_ble_gatts_app_register(0x55);
    if (err != ESP_OK) goto fail_bluedroid_enabled;

    s_initialized = true;
    ESP_LOGI(TAG, "BLE initialized: name=%s service=0x%04X", s_ble_name, CUBE_BLE_SERVICE_UUID);
    return ESP_OK;

fail_bluedroid_enabled:
    (void)esp_bluedroid_disable();
fail_bluedroid:
    (void)esp_bluedroid_deinit();
fail_controller_enabled:
    (void)esp_bt_controller_disable();
fail_controller:
    (void)esp_bt_controller_deinit();
    return err;
}

esp_err_t cube_ble_send_status(const char *text) {
    if (!text || s_conn_id == 0xFFFF || s_gatts_if == ESP_GATT_IF_NONE || s_attr_handles[2] == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_notify && !s_indicate) return ESP_ERR_INVALID_STATE;

    /* Default ATT MTU is 23, so keep a notification/indication <= 20 bytes. */
    size_t n = strlen(text);
    if (n > 20) n = 20;
    return esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_attr_handles[2],
                                       (uint16_t)n, (uint8_t *)text, s_indicate);
}

bool cube_ble_is_connected(void) { return s_conn_id != 0xFFFF; }


void cube_ble_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (cube_ble_is_connected()) {
            cube_state_snapshot_t st;
            app_state_snapshot(&st);
            if (st.ble_notify_enabled || st.ble_indicate_enabled) {
                char line[21];
                long power_w = 0;
                if (st.ccs_output_voltage_raw > 0 && st.ccs_output_current_0p1a > 0) {
                    power_w = ((long)st.ccs_output_voltage_raw * (long)st.ccs_output_current_0p1a) / 100L;
                }
                if (power_w > 9999L) power_w = 9999L;
                int current_abs = 0;
                if (st.ccs_output_current_0p1a != INT16_MIN) {
                    current_abs = st.ccs_output_current_0p1a < 0
                        ? -(int)st.ccs_output_current_0p1a
                        : (int)st.ccs_output_current_0p1a;
                }
                int n = snprintf(line, sizeof(line), "%u.%uV %d.%dA %ldW",
                                 st.ccs_output_voltage_raw / 10U, st.ccs_output_voltage_raw % 10U,
                                 st.ccs_output_current_0p1a == INT16_MIN ? 0 : st.ccs_output_current_0p1a / 10,
                                 current_abs % 10, power_w);
                if (n > 0) (void)cube_ble_send_status(line);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
