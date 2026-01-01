#include "esp_log.h"
static const char *TAG="ADC_PWM";
void adc_pwm_init(void){
    /* The original firmware uses adc_oneshot + calibration and LEDC PWM.
       Exact physical ADC channel/GPIO was not proven from the dump, therefore V2
       deliberately leaves these peripherals untouched until bench verification. */
    ESP_LOGW(TAG,"ADC/PWM safe mode: no unknown GPIO is driven");
}
