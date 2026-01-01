# FGMDCharge / FD_ROUND V2 — RELEASE 2.4.8

## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.8

1. BLE:
   - Bluedroid принудительно выбран как host.
   - Включён BLE 4.2 legacy advertising block, необходимый для используемых `esp_ble_gap_*` API.
   - BLE 5.0 advertising block отключён, чтобы не оставлять несовместимую конфигурацию legacy API.
   - GATTS остаётся включённым.

2. Web UI:
   - Убрана зависимость от linker-generated `_binary_web_*` символов.
   - HTML/CSS/JS теперь компилируются в `main/web_assets.c` как статические C-массивы.
   - Это устраняет исходную причину `undefined reference` для web assets.
   - Web UI остаётся локальным и сохраняет анимации, адаптивность и reduced-motion режим.

3. LCD/LVGL:
   - Исправлен неправильный уровень указателя в `lv_disp_flush_ready()`.
   - Был warning `lv_disp_drv_t **` вместо `lv_disp_drv_t *`.

4. Сборка:
   - `BUILD_RELEASE.bat` после `idf.py set-target` дополнительно фиксирует BLE-конфигурацию в сгенерированном `sdkconfig`.
   - Полностью очищаются старые `sdkconfig`, `build`, `managed_components`, `dependencies.lock`.

## Проверки

- host core tests: PASS
- static audit: PASS
- release verification: PASS
- оригинальный `dump1.bin`: SHA-256 проверяется
- OTA partition map: проверяется
- web assets: проверяются
- Python tools: syntax проверяется
- Windows scripts: проверяются

## Важное ограничение

В данном окружении нет пользовательского Windows ESP-IDF 5.0.9 toolchain, поэтому я не называю релиз «фактически собранным», пока `idf.py build` не выполнится на Windows.

После распаковки запускать только:

`BUILD_RELEASE.bat`

И только если он выведет `BUILD OK`, использовать:

`FLASH_RELEASE_OTA1.bat`

Не прошивать неполученный/частичный `.bin`.

5. LCD dependency:
   - Добавлен локальный `components/esp_lcd_ili9341`; это устраняет предыдущий `esp_lcd_ili9341.h: No such file or directory` независимо от Component Registry.

## Финальная проверка исходников

- `components/esp_lcd_ili9341` встроен в проект и совместим с API `esp_lcd_panel_t` ESP-IDF 5.0.9: без `disp_sleep`.
- HTML/CSS/JS заново сгенерированы в `main/web_assets.c`; массивы сверяются статическим аудитом.
- Web UI содержит защищённую ручную отправку CAN-кадра и контролируемый OTA с прогрессом.
- Исправлены пограничные значения `INT16_MIN` для отображения/уведомлений.
- `main/app_state.c` явно подключает `<limits.h>`.
