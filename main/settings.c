#include "settings.h"
#include <string.h>
#include <stdio.h>
#include "nvs.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "SETTINGS";
#define CUBE_NVS_BRIGHTNESS "brightness"

esp_err_t settings_init(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(CUBE_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS namespace '%s' open failed: %s", CUBE_NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    size_t len = 0;
    err = nvs_get_str(h, CUBE_NVS_BLE_NAME, NULL, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_str(h, CUBE_NVS_BLE_NAME, CUBE_BLE_NAME);
        if (err == ESP_OK) err = nvs_commit(h);
        if (err == ESP_OK) ESP_LOGI(TAG, "Created default BLE name '%s' in %s", CUBE_BLE_NAME, CUBE_NVS_NAMESPACE);
    } else if (err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH) {
        err = ESP_OK;
    }
    nvs_close(h);
    if (err == ESP_OK) ESP_LOGI(TAG, "NVS namespace '%s' ready", CUBE_NVS_NAMESPACE);
    return err;
}

esp_err_t settings_get_ble_name(char *out, size_t out_size)
{
    if (!out || out_size < 2U) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(CUBE_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t len = out_size;
    err = nvs_get_str(h, CUBE_NVS_BLE_NAME, out, &len);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        (void)snprintf(out, out_size, "%s", CUBE_BLE_NAME);
        return ESP_OK;
    }
    if (err == ESP_OK && out[0] == '\0') {
        (void)snprintf(out, out_size, "%s", CUBE_BLE_NAME);
    }
    return err;
}

esp_err_t settings_set_ble_name(const char *name)
{
    if (!name || !name[0] || strlen(name) >= 32U) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(CUBE_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, CUBE_NVS_BLE_NAME, name);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}


uint8_t settings_get_brightness(void)
{
    nvs_handle_t h;
    uint8_t value = 100;
    if (nvs_open(CUBE_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return value;
    uint8_t stored = 0;
    if (nvs_get_u8(h, CUBE_NVS_BRIGHTNESS, &stored) == ESP_OK && stored >= 10U && stored <= 100U) {
        value = stored;
    }
    nvs_close(h);
    return value;
}

esp_err_t settings_set_brightness(uint8_t percent)
{
    if (percent < 10U || percent > 100U) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(CUBE_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, CUBE_NVS_BRIGHTNESS, percent);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
