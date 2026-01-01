FGMDCharge / FD_ROUND V2 — RELEASE 2.8.2

ЧТО ИСПРАВЛЕНО В 2.5.7
- Убран ранний PWM на GPIO3 (был POWERON reboot loop)
- Реле: уровень LOW до gpio_config (без дребезга катушки)
- ST7789 + smoke fill (как в стабильном 2.1.2)
- Компактная вёрстка UI под 240x280
- Открытая точка FGMDCharge-V2, канал 6, max TX, SoftAP до BLE


ЧТО ИСПРАВЛЕНО В 2.5.7
- DISPLAY: lcd_flush_done больше не вызывает lv_disp_flush_ready до регистрации LVGL
  (устранён Guru Meditation Store access fault на smoke fill)


ЧТО ИСПРАВЛЕНО В 2.5.7
- Web: URI /* перенесён в конец (ESP_ERR_HTTPD_HANDLER_EXISTS устранён)
- LCD: ST7789 240x280, gap_y=20, invert, BGR + smoke fill при старте
- SoftAP FGMDCharge-V2 / FDcube2026 (ssid_hidden=0)

=======================================

Полная реконструкция логики оригинальной прошивки кубика + веб-панель.

ЧТО ДОБАВЛЕНО В 2.5.7
---------------------
1. Полный state machine GB/T battery-side (состояния 0..7).
2. Реле по аналитике RE:
   - State4: GPIO6 = HIGH (prepare / pre-charge)
   - State5 + 10 с + substate==3: GPIO6=LOW, GPIO7=HIGH (основная силовая)
   - Stop/Idle/Error: оба реле = LOW (fail-safe)
3. Активная передача CAN: BHM, BRO, BCL, BCS, BSM, BST (при включённом TX).
4. BLE-команды 0x66 / 0x0D / 0x0E (как в оригинале).
5. Веб-панель показывает main_state, substate, реле, handoff, session.
6. Кнопки Start charge / Stop в веб-UI (требуют X-Cube-Key).
7. Исправлен критический баг 2.5.7: CUBE_LCD_BL_GPIO = -1 (GPIO2 не драйвится).

КРИТИЧЕСКИЕ ПРАВИЛА
-------------------
- GPIO2 — strapping, НЕ драйвить.
- Реле только GPIO6 и GPIO7.
- CAN TX по умолчанию ВЫКЛЮЧЕН (безопасность).
- Перед реальной зарядкой: включите TX в веб-панели, проверьте CAN-трафик.

Wi-Fi AP
-------
SSID:     FGMDCharge-V2
Password: FDcube2026
URL:      http://192.168.4.1/
API key:  FDcube2026  (заголовок X-Cube-Key)

СБОРКА
------
Нужен ESP-IDF 5.0.9 (ESP32-C3).
  idf.py set-target esp32c3
  idf.py build

Или BUILD_RELEASE.bat на Windows (если окружение IDF уже настроено).

ПРОШИВКА (безопасно в OTA1)
---------------------------
FLASH_RELEASE_OTA1.bat — пишет только app в 0x1A0000, OTA0 не трогает.
ROLLBACK_OTA0.bat — откат на старый слот.
RECOVERY_ORIGINAL.bat — полный dump1.bin.

ВНИМАНИЕ
--------
Это реконструкция по дампу. Перед подключением к реальной станции:
1) Проверьте CAN TX на логическом анализаторе / CANalyzer.
2) Убедитесь, что реле не коммутируют опасную нагрузку без контроля.
3) Смените CUBE_WEB_KEY и Wi-Fi пароль в config.h.
