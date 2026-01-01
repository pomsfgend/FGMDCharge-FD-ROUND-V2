#include "config.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "ADC_PWM";

void adc_pwm_init(void)
{
    /* Original FW: ADC1_CH0 on GPIO0 (measurement) + LEDC ~60 kHz 10-bit on GPIO3.
       GPIO3 is the only free high-rate PWM pin; use it as LCD backlight. */
#if (CUBE_LCD_BL_PWM_GPIO >= 0)
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CUBE_LCD_BL_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&tcfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer fail: %s", esp_err_to_name(err));
        return;
    }
    ledc_channel_config_t ccfg = {
        .gpio_num = CUBE_LCD_BL_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = CUBE_LCD_BL_PWM_DUTY,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ccfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC ch fail: %s", esp_err_to_name(err));
        return;
    }
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, CUBE_LCD_BL_PWM_DUTY);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGW(TAG, "backlight PWM GPIO%d %u Hz duty=%u/1023 ON",
             CUBE_LCD_BL_PWM_GPIO, (unsigned)CUBE_LCD_BL_PWM_HZ, (unsigned)CUBE_LCD_BL_PWM_DUTY);
#else
    ESP_LOGW(TAG, "ADC/PWM: backlight PWM disabled");
#endif
}
