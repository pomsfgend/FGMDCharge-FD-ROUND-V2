#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "app_state.h"
#include "can_gbt.h"
#include "display.h"
#include "touch_cst816s.h"
#include "ble.h"
#include "web.h"
#include "settings.h"
#include "config.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "esp_timer.h"
#include "sm_relay.h"

static const char *TAG = "MAIN";
void adc_pwm_init(void);

static void myTask_Timer(void *arg)
{
    (void)arg;
    for (;;) {
        app_state_housekeeping();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void myTask_SM(void *arg)
{
    (void)arg;
    for (;;) {
        sm_relay_tick();
        vTaskDelay(pdMS_TO_TICKS(CUBE_SM_TIMEBASE_MS));
    }
}


static void boot_select_on_reset(void)
{
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK) return;
    uint8_t slot = 1;
    uint8_t count = 0;
    int64_t last = 0;
    nvs_get_u8(h, "boot_slot", &slot);
    nvs_get_u8(h, "rst_count", &count);
    nvs_get_i64(h, "rst_us", &last);
    int64_t now = esp_timer_get_time();
    esp_reset_reason_t why = esp_reset_reason();
    if (why == ESP_RST_POWERON || why == ESP_RST_EXT || why == ESP_RST_SW) {
        count = (now - last < 8000000) ? (uint8_t)(count + 1) : 1;
        nvs_set_u8(h, "rst_count", count);
        nvs_set_i64(h, "rst_us", now);
        nvs_commit(h);
        if (count >= 3) {
            slot = slot ? 0 : 1;
            nvs_set_u8(h, "boot_slot", slot);
            nvs_set_u8(h, "rst_count", 0);
            nvs_commit(h);
            esp_partition_subtype_t sub = slot ? ESP_PARTITION_SUBTYPE_APP_OTA_1 : ESP_PARTITION_SUBTYPE_APP_OTA_0;
            const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, sub, NULL);
            if (part) esp_ota_set_boot_partition(part);
            nvs_close(h);
            esp_restart();
        }
    }
    nvs_close(h);
}

void app_main(void) {
    ESP_LOGW(TAG, "boot %s %s", CUBE_FW_NAME, CUBE_FW_VERSION);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    boot_select_on_reset();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return;
    }

    app_state_init();

    err = settings_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "settings init failed: %s", esp_err_to_name(err));

    err = sm_relay_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "sm_relay init failed: %s", esp_err_to_name(err));
    else ESP_LOGW(TAG, "stage: relays OK (both OFF)");

    adc_pwm_init();
    ESP_LOGW(TAG, "stage: adc/pwm done");

    err = gbt_can_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CAN init failed: %s", esp_err_to_name(err));
    } else if (xTaskCreate(gbt_can_task, "myTask_CAN", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "CAN task creation failed");
    } else {
        ESP_LOGW(TAG, "stage: CAN task ok");
    }

    if (xTaskCreate(myTask_SM, "myTask_SM", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "SM task creation failed");
    } else {
        ESP_LOGW(TAG, "stage: SM 10ms task ok");
    }

    err = touch_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(err));
    else ESP_LOGW(TAG, "stage: touch ok");

    err = display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s", esp_err_to_name(err));
    } else if (xTaskCreate(display_task, "myTask_lcd", 6144, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "display task creation failed");
    } else {
        ESP_LOGW(TAG, "stage: LCD/LVGL ok");
    }

    /* SoftAP first for scan visibility, then BLE. */
    err = web_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "Web init failed: %s", esp_err_to_name(err));
    else ESP_LOGW(TAG, "stage: Web AP ok");

    #if CUBE_BLE_ENABLE
    err = cube_ble_init();
#else
    err = ESP_ERR_NOT_SUPPORTED;
#endif
    if (err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "stage: BLE disabled");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(err));
    } else if (xTaskCreate(cube_ble_task, "myTask_BLE", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "BLE task creation failed");
    } else {
        ESP_LOGW(TAG, "stage: BLE ok");
    }

    if (xTaskCreate(myTask_Timer, "myTask_Timer", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Timer task creation failed");
    }

    ESP_LOGW(TAG, "READY %s CAN_TX=%d WiFi=%s key=%s", CUBE_FW_VERSION,
             gbt_can_get_tx_enabled(), CUBE_WIFI_SSID, CUBE_WEB_KEY);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
