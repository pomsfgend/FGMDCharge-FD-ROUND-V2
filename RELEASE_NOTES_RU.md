# FGMDCharge / FD_ROUND V2 — Release 2.0.6

## 2.0.6 — полный инженерный релиз

Этот архив предназначен для чистой сборки на Windows 11 с ESP-IDF 5.0.9 и ESP32-C3.
Он объединяет исходники проекта, безопасный OTA-путь, оригинальный 4 MiB dump,
скрипты обслуживания и локальную веб-панель.

Исправления:

- устранена предыдущая ошибка линковки `esp_ble_gap_*` через явное включение Bluedroid BLE;
- устранена ошибка линковки `_binary_web_*`: HTML/CSS/JS теперь компилируются в `main/web_assets.c` и подключаются как обычный исходник;
- исправлена GATT service-table структура и CCCD;
- advertising data хранится в статической памяти и может безопасно использоваться асинхронно;
- BLE status payload ограничивается 20 байтами при стандартном ATT MTU;
- LVGL flush-ready вызывается именно по callback завершения LCD transfer;
- touch callback зарегистрирован в LVGL и сохраняет последнее положение при чтении с ошибкой;
- CAN использует штатный timing macro 250 kbit/s и BUS-OFF recovery;
- OTA пишет только следующий app partition и завершает запись через `esp_ota_end()` до переключения boot partition;
- HTTP body receive переживает `HTTPD_SOCK_ERR_TIMEOUT` через повтор чтения;
- OTA-панель показывает реальный progress через `XMLHttpRequest.upload.onprogress`;
- усилена валидация Raw CAN и OTA input;
- host tests исправлены для актуального Python (`dataclasses.field(default_factory=...)`) и расширены тестом максимального GB/T payload;
- добавлена единая `verify_release.py`, проверяющая файлы, конфигурацию, partition map, source hygiene и оригинальный dump;
- добавлена `verify_original_dump.py` для запрета случайного полного восстановления из изменённого файла.

## Интерфейс

Web UI переработан как локальная dashboard-панель: glass cards, animated background glows,
status pills, адаптивная мобильная верстка, плавные hover/transition эффекты и
`prefers-reduced-motion` fallback. Внешних сетевых зависимостей нет.

## Ограничения, которые остаются намеренными

Точные ADC/PWM выводы и оригинальный BLE OTA framing не считаются доказанными только по dump1.bin.
Поэтому прошивка не отправляет неизвестные силовые команды автоматически и не принимает
произвольные BLE-пакеты как OTA.

## Источник оригинала

`original/dump1.bin`:
`1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233`

## Самодостаточный LCD-компонент

В релиз добавлен локально vendored компонент `components/esp_lcd_ili9341`, чтобы сборка не зависела от успешного скачивания ILI9341 через Component Registry. Интерфейс компонента совместим с `esp_lcd` и ESP-IDF 5.x.
