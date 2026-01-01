#include "config.h"
#include "esp_log.h"

#if (CUBE_LCD_BL_PWM_GPIO >= 0)
#include "driver/ledc.h"
#endif

static const char *TAG = "ADC_PWM";

void adc_pwm_init(void)
{
#if (CUBE_LCD_BL_PWM_GPIO >= 0)
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CUBE_LCD_BL_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&tcfg) != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer fail");
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
    if (ledc_channel_config(&ccfg) != ESP_OK) {
        ESP_LOGE(TAG, "LEDC ch fail");
        return;
    }
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, CUBE_LCD_BL_PWM_DUTY);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGW(TAG, "backlight PWM GPIO%d ON", CUBE_LCD_BL_PWM_GPIO);
#else
    /* Keep peripherals untouched until bench-confirmed. GPIO3 PWM caused
       POWERON reboot loops on this hardware when enabled at boot. */
    ESP_LOGW(TAG, "ADC/PWM safe mode (no GPIO driven)");
#endif
}
