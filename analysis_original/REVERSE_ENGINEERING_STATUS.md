# Статус работ

## Подтверждено

- [x] full 4 MiB flash dump
- [x] partition table
- [x] ESP32-C3
- [x] ESP-IDF 5.0.9
- [x] project name FD_ROUND
- [x] OTA layout
- [x] source filenames embedded in binary
- [x] FreeRTOS tasks and priorities
- [x] ILI9341 SPI pins and speed
- [x] CAN/TWAI pins and 250 kbit/s
- [x] acceptance filter = accept all
- [x] GB/T-style CAN IDs
- [x] LVGL presence
- [x] BLE presence
- [x] BLE OTA presence
- [x] NVS BLEname / Alldata

## В работе / следующие этапы

- [ ] complete GATT attribute UUID/handle reconstruction
- [ ] exact ADC channel/GPIO and conversion equations
- [ ] exact PWM channel/GPIO/frequency
- [ ] complete GB/T RX field map
- [ ] complete GB/T TX/state machine
- [ ] identify touch controller
- [ ] identify 79 KiB ota_0 tail
- [ ] reproduce LVGL display flush callback exactly

## Правило качества

До подтверждения измерением/дизассемблером неизвестные параметры не добавлять в production build.
