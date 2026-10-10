#pragma once

/* Original FD_ROUND cube, ESP32-C3. Do not use these pins on an S3 module. */

#ifndef CUBE_LCD_USE_ST7789
#define CUBE_LCD_USE_ST7789          1
#endif
#ifndef CUBE_LCD_H_RES
#define CUBE_LCD_H_RES               240
#endif
#ifndef CUBE_LCD_V_RES
#define CUBE_LCD_V_RES               280
#endif
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

#define CUBE_TOUCH_ENABLED_DEFAULT   1
#define CUBE_TOUCH_SDA_GPIO          1
#define CUBE_TOUCH_SCL_GPIO          8
#define CUBE_TOUCH_I2C_PORT          0
#define CUBE_TOUCH_ADDR              0x15
#define CUBE_TOUCH_SWAP_XY           0
#define CUBE_TOUCH_INVERT_X          0
#define CUBE_TOUCH_INVERT_Y          0
#define CUBE_TOUCH_POLL_MS           15

#define CUBE_CAN_TX_GPIO             10
#define CUBE_CAN_RX_GPIO             9
#define CUBE_CAN_BITRATE             250000

#define CUBE_RELAY_A_GPIO            6
#define CUBE_RELAY_B_GPIO            7
