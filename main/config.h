#pragma once

#define CUBE_FW_NAME                 "FD_ROUND_V2"
#define CUBE_FW_VERSION              "2.8.4"

/* SoftAP */
#define CUBE_WEB_KEY                 ""
#define CUBE_WIFI_SSID               "Cube"
#define CUBE_WIFI_PASSWORD           "12345678"
#define CUBE_WIFI_CHANNEL            1
#define CUBE_WIFI_MAX_CONNECTIONS    4
#define CUBE_WIFI_OPEN               0

#define CUBE_TX_COMPILED_ENABLE      1
#define CUBE_TX_DEFAULT_ENABLED      0
#define CUBE_HTTP_MAX_UPLOAD       (0x190000)

/* LCD ST7789 240x280 */
#define CUBE_LCD_USE_ST7789          1
#define CUBE_LCD_H_RES               240
#define CUBE_LCD_V_RES               280
#define CUBE_LCD_MOSI_GPIO           5
#define CUBE_LCD_MISO_GPIO           (-1)
#define CUBE_LCD_SCLK_GPIO           19
#define CUBE_LCD_CS_GPIO             4
#define CUBE_LCD_DC_GPIO             18
#define CUBE_LCD_SPI_HZ              40000000
#define CUBE_LCD_BL_GPIO             2
#define CUBE_LCD_BL_PWM_GPIO         2
#define CUBE_LCD_BL_PWM_HZ           5000
#define CUBE_LCD_BL_PWM_DUTY_DEFAULT 70
#define CUBE_LCD_RST_GPIO            (-1)
#define CUBE_STATUS_LED_GPIO         (-1)
#define CUBE_LCD_QUEUE_DEPTH         10
#define CUBE_LCD_MAX_TRANSFER        38400
#define CUBE_LCD_GAP_X               0
#define CUBE_LCD_GAP_Y               20
#define CUBE_LCD_INVERT              1
#define CUBE_LCD_BGR                 1
#define CUBE_LCD_MIRROR_X            0
#define CUBE_LCD_MIRROR_Y            0

/* Touch CST816x */
#define CUBE_TOUCH_ENABLED_DEFAULT   1
#define CUBE_TOUCH_SDA_GPIO          1
#define CUBE_TOUCH_SCL_GPIO          8
#define CUBE_TOUCH_I2C_PORT          0
#define CUBE_TOUCH_ADDR              0x15
#define CUBE_TOUCH_SWAP_XY           0
#define CUBE_TOUCH_INVERT_X          0
#define CUBE_TOUCH_INVERT_Y          0
#define CUBE_TOUCH_POLL_MS           15

/* CAN */
#define CUBE_CAN_TX_GPIO             10
#define CUBE_CAN_RX_GPIO             9
#define CUBE_CAN_BITRATE             250000

/* Relays */
#define CUBE_RELAY_A_GPIO            6
#define CUBE_RELAY_B_GPIO            7

/* BLE off by default */
#define CUBE_BLE_ENABLE              0
#define CUBE_BLE_NAME                "FGMDCharge"

/* LVGL performance */
#define CUBE_LVGL_TICK_MS            5
#define CUBE_LVGL_TASK_MS            10
#define CUBE_LVGL_TASK_PRIO          4

/* State machine / CAN queues (required by app_state, can_gbt, sm_relay) */
#define CUBE_SM_TIMEBASE_MS              10
#define CUBE_CAN_TX_QUEUE                16
#define CUBE_CAN_RX_QUEUE                32
#define CUBE_CAN_HISTORY                 48
#define CUBE_RELAY_HANDOFF_MS            300

/* BCL defaults: 400.0 V / 10.0 A, max 450.0 V */
#define CUBE_DEFAULT_REQ_VOLTAGE_0P1V    2400
#define CUBE_DEFAULT_REQ_CURRENT_0P1A    500
#define CUBE_DEFAULT_MAX_VOLTAGE_0P1V    4500

/* NVS */
#define CUBE_NVS_NAMESPACE               "storage"
#define CUBE_NVS_BLE_NAME                "ble_name"
#define CUBE_NVS_ALLDATA                 "alldata"

/* BLE GATT (Bluedroid compiled, start gated by CUBE_BLE_ENABLE) */
#define CUBE_BLE_SERVICE_UUID            0xFFE0
#define CUBE_BLE_CHAR1_UUID              0xFFE1
#define CUBE_BLE_CHAR2_UUID              0xFFE2
#define CUBE_BLE_CHAR3_UUID              0xFFE3

/* alias used by adc_pwm.c */
#define CUBE_LCD_BL_PWM_DUTY             CUBE_LCD_BL_PWM_DUTY_DEFAULT
