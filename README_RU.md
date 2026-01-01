# FGMDCharge / FD_ROUND V2 — Release 2.7.0

Канонический Windows-релиз для ESP32-C3 на ESP-IDF 5.0.9.

## Один путь

1. Распаковать архив в короткий ASCII-путь, например `C:\FGMDCharge_FD_ROUND_V2_RELEASE`.
2. Запустить `BUILD_RELEASE.bat`.
3. Дождаться `BUILD OK`.
4. Запустить `FLASH_RELEASE_OTA1.bat` и подтвердить запись только в OTA1.
5. После загрузки подключиться к Wi-Fi `FGMDCharge-V2`, открыть `http://192.168.4.1/`.

Первичное обновление не стирает OTA0 и перед записью сохраняет полный 4 MiB backup.

## Что исправлено в 2.5.7

- Исправлена линковка Bluedroid BLE: стек и legacy BLE 4.2 advertising явно включены в `sdkconfig.defaults`.
- HTML/CSS/JS компилируются в `main/web_assets.c` как статические массивы; linker `EMBED_*` символы не используются.
- BLE GATT service/characteristic table приведена к service-table формату ESP-IDF.
- Буфер advertising сделан постоянным для асинхронного GAP API.
- BLE payload ограничен 20 байтами для стандартного ATT MTU.
- LCD/LVGL flush-ready перенесён в callback завершения DMA-передачи; используется реально зарегистрированный `lv_disp_t`.
- Touch input подключён к LVGL и переводится в безопасный N/A при отсутствии контроллера.
- CAN использует `TWAI_TIMING_CONFIG_250KBITS()` и автоматическое восстановление после BUS-OFF.
- OTA проверяет magic, размер, целевой app partition и `esp_ota_end()` до выбора нового boot partition.
- Web request reader повторяет socket timeout, а OTA имеет проверку размера/ESP image magic.
- Web OTA использует настоящий `XMLHttpRequest` progress вместо фиктивного прогресса.
- Raw CAN ввод на панели стал строгим; TX остаётся выключенным по умолчанию.
- Добавлены host regression tests, static audit и проверка SHA-256 оригинального dump.

## Веб-интерфейс

Панель локальная и автономная: без CDN, внешних шрифтов и внешних JS-библиотек.
Она содержит телеметрию GB/T, состояние CAN/BLE/touch, сервисную Raw CAN отправку,
защищённый X-Cube-Key шлюз TX и OTA в неактивный слот.

## Аппаратная карта

- ESP32-C3
- CAN/TWAI 250 kbit/s: TX GPIO10 / RX GPIO9
- ILI9341: MOSI GPIO5 / SCLK GPIO19 / CS GPIO4 / DC GPIO18
- Touch I2C: SDA GPIO1 / SCL GPIO8 / address `0x15`
- BLE: `FGMDCharge`, service `FFE0`, characteristics `FFE1..FFE3`

## Безопасность

`CUBE_TX_DEFAULT_ENABLED` равен `0`. Для управления TX и Raw CAN требуется `CUBE_WEB_KEY`.
Перед работой на силовой линии проверьте распиновку и протокол на лабораторном стенде.

## Оригинал

`original/dump1.bin` — исходный 4 MiB dump.
SHA-256: `1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233`.

Полный сценарий описан в `README_START_HERE_RU.txt`.
