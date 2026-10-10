#pragma once

/*
 * ESP32-S3-DevKitC-1 N16R8 (and N8R2, same free pins).
 * SPI2, CST816 on I2C0, TWAI 250 kbit, two active-high relay outputs.
 *
 * Not used, on purpose:
 *   0, 3, 45, 46     strapping
 *   19, 20           USB D-/D+  (the port this board is flashed from)
 *   22-25            not bonded on ESP32-S3
 *   26-32            in-package flash
 *   33-37            octal PSRAM on N16R8 / N8R8
 *   43, 44           UART0 (CH340 clones)
 *   38, 48           onboard RGB, revision-dependent
 */

#ifndef CUBE_LCD_USE_ST7789
#define CUBE_LCD_USE_ST7789          1
#endif
#ifndef CUBE_LCD_H_RES
#define CUBE_LCD_H_RES               240
#endif
#ifndef CUBE_LCD_V_RES
#define CUBE_LCD_V_RES               280
#endif
#ifndef CUBE_LCD_MOSI_GPIO
#define CUBE_LCD_MOSI_GPIO           11
#endif
#ifndef CUBE_LCD_MISO_GPIO
#define CUBE_LCD_MISO_GPIO           (-1)
#endif
#ifndef CUBE_LCD_SCLK_GPIO
#define CUBE_LCD_SCLK_GPIO           12
#endif
#ifndef CUBE_LCD_CS_GPIO
#define CUBE_LCD_CS_GPIO             10
#endif
#ifndef CUBE_LCD_DC_GPIO
#define CUBE_LCD_DC_GPIO             13
#endif
#ifndef CUBE_LCD_SPI_HZ
#define CUBE_LCD_SPI_HZ              40000000
#endif
#ifndef CUBE_LCD_BL_GPIO
#define CUBE_LCD_BL_GPIO             21
#endif
#ifndef CUBE_LCD_BL_PWM_GPIO
#define CUBE_LCD_BL_PWM_GPIO         21
#endif
#ifndef CUBE_LCD_BL_PWM_HZ
#define CUBE_LCD_BL_PWM_HZ           5000
#endif
#ifndef CUBE_LCD_BL_PWM_DUTY_DEFAULT
#define CUBE_LCD_BL_PWM_DUTY_DEFAULT 70
#endif
#ifndef CUBE_LCD_RST_GPIO
#define CUBE_LCD_RST_GPIO            14
#endif
#ifndef CUBE_STATUS_LED_GPIO
#define CUBE_STATUS_LED_GPIO         47
#endif
#ifndef CUBE_LCD_QUEUE_DEPTH
#define CUBE_LCD_QUEUE_DEPTH         10
#endif
#ifndef CUBE_LCD_MAX_TRANSFER
#define CUBE_LCD_MAX_TRANSFER        38400
#endif
#ifndef CUBE_LCD_GAP_X
#define CUBE_LCD_GAP_X               0
#endif
#ifndef CUBE_LCD_GAP_Y
#define CUBE_LCD_GAP_Y               20
#endif
#ifndef CUBE_LCD_INVERT
#define CUBE_LCD_INVERT              1
#endif
#ifndef CUBE_LCD_BGR
#define CUBE_LCD_BGR                 1
#endif
#ifndef CUBE_LCD_MIRROR_X
#define CUBE_LCD_MIRROR_X            0
#endif
#ifndef CUBE_LCD_MIRROR_Y
#define CUBE_LCD_MIRROR_Y            0
#endif

#ifndef CUBE_TOUCH_ENABLED_DEFAULT
#define CUBE_TOUCH_ENABLED_DEFAULT   1
#endif
#ifndef CUBE_TOUCH_SDA_GPIO
#define CUBE_TOUCH_SDA_GPIO          1
#endif
#ifndef CUBE_TOUCH_SCL_GPIO
#define CUBE_TOUCH_SCL_GPIO          2
#endif
#ifndef CUBE_TOUCH_I2C_PORT
#define CUBE_TOUCH_I2C_PORT          0
#endif
#ifndef CUBE_TOUCH_ADDR
#define CUBE_TOUCH_ADDR              0x15
#endif
#ifndef CUBE_TOUCH_SWAP_XY
#define CUBE_TOUCH_SWAP_XY           0
#endif
#ifndef CUBE_TOUCH_INVERT_X
#define CUBE_TOUCH_INVERT_X          0
#endif
#ifndef CUBE_TOUCH_INVERT_Y
#define CUBE_TOUCH_INVERT_Y          0
#endif
#ifndef CUBE_TOUCH_POLL_MS
#define CUBE_TOUCH_POLL_MS           15
#endif

#ifndef CUBE_CAN_TX_GPIO
#define CUBE_CAN_TX_GPIO             17
#endif
#ifndef CUBE_CAN_RX_GPIO
#define CUBE_CAN_RX_GPIO             18
#endif
#ifndef CUBE_CAN_BITRATE
#define CUBE_CAN_BITRATE             250000
#endif

#ifndef CUBE_RELAY_A_GPIO
#define CUBE_RELAY_A_GPIO            6
#endif
#ifndef CUBE_RELAY_B_GPIO
#define CUBE_RELAY_B_GPIO            7
#endif

/* XPT2046 resistive touch, same SPI bus, its own CS. Not wired in touch_cst816s.c. */
#define CUBE_XPT_CS_GPIO             16
#define CUBE_XPT_IRQ_GPIO            8
