#include "diag.h"

#include <string.h>
#include "driver/temperature_sensor.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "DIAG";
static temperature_sensor_handle_t s_temp;
static uint8_t s_cpu_load;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static uint64_t s_prev_total;
static uint64_t s_prev_idle;
static bool s_ready;
static uint32_t s_last_sample_ms;
static float s_cached_temp;
static bool s_cached_temp_valid;

esp_err_t diag_init(void)
{
    if (s_ready) return ESP_OK;

    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    esp_err_t err = temperature_sensor_install(&cfg, &s_temp);
    if (err == ESP_OK) {
        err = temperature_sensor_enable(s_temp);
        if (err != ESP_OK) {
            temperature_sensor_uninstall(s_temp);
            s_temp = NULL;
        }
    }
    if (err != ESP_OK) {
        s_temp = NULL;
        ESP_LOGW(TAG, "CPU temperature sensor unavailable: %s", esp_err_to_name(err));
    }

    s_ready = true;
    return ESP_OK;
}

bool diag_get_temperature(float *out_c)
{
    if (!out_c) return false;
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if ((uint32_t)(now - s_last_sample_ms) >= 500U || s_last_sample_ms == 0U) {
        if (s_temp) {
            float value = 0.0f;
            s_cached_temp_valid = temperature_sensor_get_celsius(s_temp, &value) == ESP_OK;
            if (s_cached_temp_valid) s_cached_temp = value;
        } else {
            s_cached_temp_valid = false;
        }
        s_last_sample_ms = now;
    }
    if (!s_cached_temp_valid) return false;
    *out_c = s_cached_temp;
    return true;
}

uint8_t diag_get_cpu_load_percent(void)
{
    static uint32_t last_cpu_ms;
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if (last_cpu_ms != 0U && (uint32_t)(now - last_cpu_ms) < 500U) return s_cpu_load;
    last_cpu_ms = now;

    const UBaseType_t cap = 32;
    TaskStatus_t tasks[cap];
    uint32_t total_runtime = 0;
    UBaseType_t count = uxTaskGetSystemState(tasks, cap, &total_runtime);
    if (count == 0 || total_runtime == 0) return s_cpu_load;

    uint32_t idle_runtime = 0;
    for (UBaseType_t i = 0; i < count; ++i) {
        if (strncmp(tasks[i].pcTaskName, "IDLE", 4) == 0) {
            idle_runtime += tasks[i].ulRunTimeCounter;
        }
    }

    taskENTER_CRITICAL(&s_lock);
    uint32_t dt_total = total_runtime - (uint32_t)s_prev_total;
    uint32_t dt_idle = idle_runtime - (uint32_t)s_prev_idle;
    s_prev_total = total_runtime;
    s_prev_idle = idle_runtime;
    if (dt_total > 0) {
        uint32_t idle_pct = (uint32_t)(((uint64_t)dt_idle * 100ULL) / dt_total);
        if (idle_pct > 100U) idle_pct = 100U;
        s_cpu_load = (uint8_t)(100U - idle_pct);
    }
    uint8_t result = s_cpu_load;
    taskEXIT_CRITICAL(&s_lock);
    return result;
}
