#pragma once

#define CUBE_FW_NAME                 "FD_ROUND_S3"
#define CUBE_FW_VERSION              "3.0.0"

/* SoftAP — same as 2.8.15 so phones keep one bookmark. */
#define CUBE_WEB_KEY                 ""
#define CUBE_WIFI_SSID               "Cube"
#define CUBE_WIFI_PASSWORD           "12345678"
#define CUBE_WIFI_CHANNEL            6
#define CUBE_WIFI_MAX_CONNECTIONS    4
#define CUBE_WIFI_OPEN               0

#define CUBE_TX_COMPILED_ENABLE      1
#define CUBE_TX_DEFAULT_ENABLED      1
/* Log still sits in the erased tail after ota_1. Partition table is unchanged. */
#define CUBE_LOG_FLASH_BASE          0x330000
#define CUBE_LOG_SLOTS               8
#define CUBE_LOG_SLOT_SIZE           4096
#define CUBE_HTTP_MAX_UPLOAD       (0x190000)

/*
 * Board profiles. This branch defaults to a blank ESP32-S3-DevKitC-1.
 * The original cube stays on branch src/2.8.15. To rebuild that pinout
 * from this tree anyway: -DCUBE_BOARD=CUBE_BOARD_C3_ORIGINAL
 * and set-target esp32c3. Do not flash the S3 sdkconfig into the cube.
 */
#define CUBE_BOARD_C3_ORIGINAL       1
#define CUBE_BOARD_S3_DEVKIT         2
#ifndef CUBE_BOARD
#define CUBE_BOARD                   CUBE_BOARD_S3_DEVKIT
#endif

#if CUBE_BOARD == CUBE_BOARD_C3_ORIGINAL
#include "boards/esp32c3_cube.h"
#elif CUBE_BOARD == CUBE_BOARD_S3_DEVKIT
#include "boards/esp32s3_devkit.h"
#else
#error "Unknown CUBE_BOARD"
#endif

/* BLE off by default. Bluedroid stays compiled, same as 2.8.15. */
#define CUBE_BLE_ENABLE              0
#define CUBE_BLE_NAME                "FGMDCharge"

#define CUBE_LVGL_TICK_MS            5
#define CUBE_LVGL_TASK_MS            10
#define CUBE_LVGL_TASK_PRIO          4

#define CUBE_SM_TIMEBASE_MS              10
#define CUBE_CAN_TX_QUEUE                16
#define CUBE_CAN_RX_QUEUE                32
#define CUBE_CAN_HISTORY                 48
#define CUBE_RELAY_HANDOFF_MS            300

/* 240.0 V / 50.0 A, ceiling 450.0 V. The comment in 2.8.15 that said 400 V / 10 A was stale. */
#define CUBE_DEFAULT_REQ_VOLTAGE_0P1V    2400
#define CUBE_DEFAULT_REQ_CURRENT_0P1A    500
#define CUBE_DEFAULT_MAX_VOLTAGE_0P1V    4500

#define CUBE_NVS_NAMESPACE               "storage"
#define CUBE_NVS_BLE_NAME                "ble_name"
#define CUBE_NVS_ALLDATA                 "alldata"

#define CUBE_BLE_SERVICE_UUID            0xFFE0
#define CUBE_BLE_CHAR1_UUID              0xFFE1
#define CUBE_BLE_CHAR2_UUID              0xFFE2
#define CUBE_BLE_CHAR3_UUID              0xFFE3

#define CUBE_LCD_BL_PWM_DUTY             CUBE_LCD_BL_PWM_DUTY_DEFAULT
