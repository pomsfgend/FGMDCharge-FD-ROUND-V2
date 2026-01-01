# FGMDCharge FD_ROUND V2 — UI/DIAG fixes on 2.2.9 base

Изменения сделаны поверх исходного проекта V2.2.9 без восстановления/переписывания силовой CAN state machine.

## Исправлено

- Web UI: подключён внешний `/style.css`; CSP больше не блокирует оформление.
- Web UI полностью переработан под ПК и телефон.
- В Web UI добавлены CPU temperature, CPU load, свободная RAM и яркость.
- Яркость дисплея 10–100% регулируется с сайта и TFT, сохраняется в NVS.
- Backlight PWM используется только после завершения boot/инициализации панели на GPIO2.
- Touch CST816x: I2C 400 kHz вместо 100 kHz, timeout 5 ms.
- LVGL input polling: 10 ms.
- LVGL/display handler: 20 ms вместо 100 ms.
- Экранная Wi-Fi клавиатура увеличена до 150 px по высоте и получила отдельную кнопку «СКРЫТЬ КЛАВИАТУРУ».
- Диагностика CPU temperature вынесена в отдельный модуль.
- CPU load рассчитывается по FreeRTOS runtime statistics и кэшируется, чтобы сама диагностика не создавала лишнюю нагрузку.
- Температура кэшируется с периодом 500 ms.
- Stage age передаётся с устройства корректно, без сравнения времени ESP32 с временем браузера.

## Не изменено

- CAN/TWAI: TX GPIO10, RX GPIO9, 250 kbit/s.
- LCD: MOSI5, SCLK19, CS4, DC18, 20 MHz, 240x280.
- Touch: SDA1, SCL8, address 0x15.
- Relay GPIO6/GPIO7.
- CAN TX по умолчанию выключен.
- GB/T протокол и силовая state machine не расширялись.

## Проверки

- `tools/static_audit.py`: PASS.
- `host_tests/test_core.py`: PASS.
- Полный `idf.py build` в среде анализа не выполнялся: Windows ESP-IDF toolchain находится на машине пользователя.
