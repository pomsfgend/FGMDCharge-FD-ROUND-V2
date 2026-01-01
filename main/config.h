#pragma once

#define CUBE_FW_NAME                 "FD_ROUND_V2"
#define CUBE_FW_VERSION              "2.2.0"

/* Web API protection. Change before installing on a real vehicle/charging setup. */
#define CUBE_WEB_KEY                 ""  /* no API key on local AP */
#define CUBE_WIFI_SSID               "FGMDCharge-V2"
#define CUBE_WIFI_PASSWORD           ""  /* open — as 2.0.5 working */
#define CUBE_WIFI_CHANNEL            1
#define CUBE_WIFI_MAX_CONNECTIONS    4

/* Safety: boots with CAN TX disabled until explicitly enabled via web/BLE. */
#define CUBE_TX_COMPILED_ENABLE      1
#define CUBE_TX_DEFAULT_ENABLED      0

/* LCD — dump strings said ILI9341, but panel is 240x280 (ST7789 family).
 * Many cubes use ST7789 with ILI-compatible branding. Prefer ST7789. */
#define CUBE_LCD_USE_ST7789          1  /* 240x280 = ST7789 geometry */
/* LCD — pins confirmed from original dump (no BL GPIO in original). */
#define CUBE_LCD_H_RES               240
#define CUBE_LCD_V_RES               280
#define CUBE_LCD_MOSI_GPIO           5
#define CUBE_LCD_MISO_GPIO           (-1)
#define CUBE_LCD_SCLK_GPIO           19
#define CUBE_LCD_CS_GPIO             4
#define CUBE_LCD_DC_GPIO             18
#define CUBE_LCD_SPI_HZ              20000000
/* CRITICAL: original firmware never drove a backlight GPIO.
   GPIO2 is strapping — driving it causes POWERON reboot loop.
   GPIO6/7 are power relays. Keep BL = -1. */
#define CUBE_LCD_BL_GPIO             2  /* post-init; confirmed lights panel */
/* Optional late backlight: GPIO3 LEDC. Disabled by default — caused POWERON loop on some boards. */
#define CUBE_LCD_BL_PWM_GPIO         (-1)
#define CUBE_LCD_BL_PWM_HZ           60000
#define CUBE_LCD_BL_PWM_DUTY         1023
#define CUBE_LCD_RST_GPIO            (-1)
/* Bench toggles — dump: mirror(0,0), no invert observed; 240x280 may need gap. */
#define CUBE_LCD_INVERT              1
#define CUBE_LCD_GAP_X               0
#define CUBE_LCD_GAP_Y               20
#define CUBE_LCD_MIRROR_X            0
#define CUBE_LCD_MIRROR_Y            0
#define CUBE_LCD_BGR                 1
#define CUBE_STATUS_LED_GPIO         (-1)
#define CUBE_LCD_QUEUE_DEPTH         10
#define CUBE_LCD_MAX_TRANSFER        38400

/* Touch CST816x @ 0x15 */
#define CUBE_TOUCH_ENABLED_DEFAULT   1
#define CUBE_TOUCH_SDA_GPIO          1
#define CUBE_TOUCH_SCL_GPIO          8
#define CUBE_TOUCH_I2C_PORT          0
#define CUBE_TOUCH_ADDR              0x15
#define CUBE_TOUCH_SWAP_XY           0
#define CUBE_TOUCH_INVERT_X          0
#define CUBE_TOUCH_INVERT_Y          0

/* CAN/TWAI confirmed */
#define CUBE_CAN_TX_GPIO             10
#define CUBE_CAN_RX_GPIO             9
#define CUBE_CAN_BITRATE             250000
#define CUBE_CAN_TX_QUEUE             10
#define CUBE_CAN_RX_QUEUE             10

/* Relays — CONFIRMED from RE steps 18–21 */
#define CUBE_RELAY_A_GPIO            6   /* prepare / pre-charge */
#define CUBE_RELAY_B_GPIO            7   /* main power path */
/* NEVER drive: GPIO2 (strapping) */

/* BLE identity */
#define CUBE_BLE_NAME                "FGMDCharge"
#define CUBE_BLE_SERVICE_UUID        0xFFE0
#define CUBE_BLE_CHAR1_UUID          0xFFE1
#define CUBE_BLE_CHAR2_UUID          0xFFE2
#define CUBE_BLE_CHAR3_UUID          0xFFE3

/* State machine */
#define CUBE_SM_TIMEBASE_MS          10
#define CUBE_RELAY_HANDOFF_MS        10000

#define CUBE_CAN_HISTORY              64
#define CUBE_HTTP_MAX_UPLOAD       (0x190000)

/* Default charge request values (0.1 V / 0.1 A units where applicable) */
#define CUBE_DEFAULT_REQ_VOLTAGE_0P1V  570   /* 57.0 V */
#define CUBE_DEFAULT_REQ_CURRENT_0P1A  70    /* 7.0 A  */
#define CUBE_DEFAULT_MAX_VOLTAGE_0P1V  660   /* 66.0 V */

/* Start BLE after SoftAP is visible (coex). 0 = immediate. */
#define CUBE_BLE_START_DELAY_MS      0

/* 0 = do not start BLE (max SoftAP visibility on shared RF). */
#define CUBE_BLE_ENABLE              0
