# Что изменилось в каждом архиве

Сравнены исходники и документы. Файлы `.bin` в сравнении не участвуют: это дампы, не правки кода.
Номер в имени файла и строка `CUBE_FW_VERSION` иногда расходятся. Ниже указаны оба.
Одинаковый по тексту архив не описан второй раз.
Ветка [`history`](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/tree/history) идёт по порядку архивов: один коммит — один отличающийся набор исходников, без `.bin` и без MAC.
Ветка `src/X.Y.Z` указывает на архив с точным именем этой версии. Это старый исходник, не сегодняшний `main`.

Файла 2.2.8 нет.

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.4.zip

Имя даёт номер 2.0.4. config.h: 2.0.4.
Первый снимок, текстовых файлов 83.
- `03_MONITOR.bat` — первый снимок
- `AUDIT_REPORT_RU.md` — первый снимок
- `BUILD_RELEASE.bat` — первый снимок
- `BUILD_RELEASE.ps1` — первый снимок
- `CMakeLists.txt` — первый снимок
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` — первый снимок
- `FILE_INDEX.txt` — первый снимок
- `FLASH_RELEASE_OTA1.bat` — первый снимок

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5.zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.4.zip`: документы (12), прошивка и скрипты (7), экран (6), исходники (5), загрузчик и слоты (3), сайт (2).
- `main/web_assets.c` +939 −242
- `BUILD_RELEASE.bat` +109 −70 — set "PATH_CHECK=%PROJECT_DIR: =%" / if not "%PATH_CHECK%"=="%PROJECT_DIR%" (
- `main/display.c` +81 −37 — #include <stdio.h> / #include <string.h>
- `sdkconfig.defaults` +38 −9 — # Size optimization (OTA slot is 0x190000) / # BLE: required by ble.c (legacy Bluedroid GATTS + ADV 4.2)
- `tools/ota_slot.py` +32 −12 — # after hard_reset from previous op, wait a bit / # Prefer writing a fresh otadata image without relying on a successful read
- `main/web.c` +27 −5 — (void)httpd_resp_set_hdr(req, "Connection", "close"); / httpd_resp_set_status(req, "204 No Content");
- `main/ble.c` +24 −4 — ESP_LOGE(TAG, "fallback ADV config failed: %s", esp_err_to_name(err)); / ESP_LOGW(TAG, "ADV using flags-only fallback (name not in packet)");
- `main/main.c` +13 −5 — ESP_LOGW(TAG, "boot %s %s", CUBE_FW_NAME, CUBE_FW_VERSION); / ESP_LOGW(TAG, "stage: adc/pwm done");
- `tools/force_sdkconfig.py` +12 −5 — """Force BLE + size + required LVGL fonts/img.""" / "CONFIG_BT_GATTC_ENABLE": "n",
- `main/config.h` +10 −4 — #define CUBE_FW_VERSION              "2.0.5" / #define CUBE_WEB_KEY                 ""  /* empty = no API password on local AP */
- и ещё 25 файлов

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.56.zip

Имя даёт номер 2.0.56. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5.zip`: экран (3), исходники (3), сайт (2), загрузчик и слоты (2), прошивка и скрипты (2).
- `main/web_assets.c` +242 −939
- `main/display.c` +37 −81 — (void)make_card(scr, 8, 30, 108, 78, "VOLTAGE", &s_voltage); / (void)make_card(scr, 124, 30, 108, 78, "CURRENT", &s_current);
- `tools/ota_slot.py` +12 −32 — # seq 1 boots OTA0, seq 2 boots OTA1. Mirror to both copies so an older valid copy cannot win.
- `main/web.c` +7 −27
- `main/main.c` +5 −13 — ESP_LOGI(TAG, "FD_ROUND V2 %s running; CAN TX default=%d", CUBE_FW_VERSION,
- `main/config.h` +3 −9 — #define CUBE_WEB_KEY                 "change-me-2026" / #define CUBE_WIFI_PASSWORD           "FDcube2026"  /* change before field use */
- `sdkconfig.defaults` +4 −4
- `FLASH_RELEASE_OTA1.bat` +2 −2 — python -m esptool --chip esp32c3 --port "%PORT%" read_flash 0x0 0x400000 "%BACKUP_DIR%\cube_before_v2.bin" / python -m esptool --chip esp32c3 --port "%PORT%" write_flash 0x1A0000 "%APP%"
- `tools/static_audit.py` +1 −3 — if re.search(r"\bon(?:click|load|error|input|change)\s*=", html, flags=re.I): fail("inline HTML event handler found")
- `tools/force_sdkconfig.py` +1 −1 — "CONFIG_LOG_DEFAULT_LEVEL_WARN": "y",
- и ещё 2 файлов

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (1).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.56.zip`: документы (10), прошивка и скрипты (7), загрузчик и слоты (3), экран (2), исходники (2), сайт (1).
- `BUILD_RELEASE.bat` +63 −102 — set "GIT_EXE=" / if not "%PROJECT_DIR: =%"=="%PROJECT_DIR%" (
- `sdkconfig.defaults` +105 −5 — # Size-first build (original app was ~900 KB; keep under 0x190000) / # BLE: only what ble.c needs (legacy Bluedroid GATTS + ADV 4.2)
- `main/ble.c` +4 −24
- `RELEASE_FIX_2.0.5.txt` +14 −13
- `FLASH_RELEASE_OTA1.bat` +3 −7 — if "%PORT%"=="" exit /b 1 / python -m esptool --chip esp32c3 --port "%PORT%" read-flash 0x0 0x400000 "%BACKUP_DIR%\cube_before_v2.bin"
- `tools/force_sdkconfig.py` +2 −6 — """Force BLE + size policy required by the legacy Bluedroid API and OTA slot size.""" / print("sdkconfig policy forced: Bluedroid + BLE 4.2 ADV + size optimization")
- `tools/static_audit.py` +3 −3 — VERSION = "2.0.4" / "FLASH_RELEASE_OTA1.bat": ("read-flash 0x0 0x400000", "write-flash 0x1A0000", "ota_slot.py", "backup"),
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.0.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.0.4
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.0.4 / ## Что исправлено в 2.0.4
- и ещё 15 файлов

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (2).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (1).zip`: документы (9), прошивка и скрипты (3), экран (2), загрузчик и слоты (1).
- `BUILD_RELEASE.bat` +97 −62 — set "PATH_CHECK=%PROJECT_DIR: =%" / if not "%PATH_CHECK%"=="%PROJECT_DIR%" (
- `FLASH_RELEASE_OTA1.bat` +5 −1 — if "%PORT%"=="" (
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.0.5 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.0.5
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.0.5 / ## Что исправлено в 2.0.5
- `RELEASE_MANIFEST_SHA256.txt` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.0.5 / ## 2.0.5 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.0.5
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (3).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (2).zip`: исходники (1), документы (1), прошивка и скрипты (1), экран (1).
- `sdkconfig.defaults` +32 −76 — # Size optimization (must fit 0x190000 OTA slot) / # BLE: Bluedroid GATTS + ADV 4.2 (required by ble.c)
- `RELEASE_FIX_2.0.5.txt` +10 −12
- `tools/force_sdkconfig.py` +7 −2 — """Force BLE + size + safe LVGL policy.""" / "CONFIG_LV_USE_IMG": "y",
- `BUILD_RELEASE.bat` +4 −0

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (4).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (3).zip`: исходники (1), документы (1), прошивка и скрипты (1).
- `sdkconfig.defaults` +3 −59 — # Size optimization (OTA slot is 0x190000) / # BLE: required by ble.c (legacy Bluedroid GATTS + ADV 4.2)
- `RELEASE_FIX_2.0.5.txt` +10 −10
- `tools/force_sdkconfig.py` +2 −3 — """Force BLE + size + required LVGL fonts/img.""" / print("sdkconfig policy forced: Bluedroid + size + LV fonts/img")

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (5).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (4).zip`: прошивка и скрипты (4), загрузчик и слоты (3).
- `FLASH_RELEASE_OTA1.bat` +2 −2 — python -m esptool --chip esp32c3 --port "%PORT%" read_flash 0x0 0x400000 "%BACKUP_DIR%\cube_before_v2.bin" / python -m esptool --chip esp32c3 --port "%PORT%" write_flash 0x1A0000 "%APP%"
- `tools/ota_slot.py` +2 −2
- `tools/static_audit.py` +2 −2 — "FLASH_RELEASE_OTA1.bat": ("read_flash 0x0 0x400000", "write_flash 0x1A0000", "ota_slot.py", "backup"), / "RECOVERY_ORIGINAL.bat": ("verify_original_dump.py", "write_flash 0x0", "dump1.bin"),
- `RECOVERY_ORIGINAL.bat` +1 −1 — python -m esptool --chip esp32c3 --port "%PORT%" write_flash 0x0 original\dump1.bin
- `tools/backup_flash.py` +1 −1 — "read_flash", "0x0", "0x400000", str(out)]
- `tools/flash_app_to_ota1.py` +1 −1 — "write_flash", hex(OTA_OFFSET), str(app)]
- `tools/restore_full_dump.py` +1 −1 — "write_flash", "0x0", str(dump)]

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (6).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (5).zip`: исходники (3), документы (1), сайт (1), прошивка и скрипты (1).
- `main/ble.c` +24 −4 — ESP_LOGE(TAG, "fallback ADV config failed: %s", esp_err_to_name(err)); / ESP_LOGW(TAG, "ADV using flags-only fallback (name not in packet)");
- `RELEASE_FIX_2.0.5.txt` +13 −12
- `main/main.c` +13 −5 — ESP_LOGW(TAG, "boot %s %s", CUBE_FW_NAME, CUBE_FW_VERSION); / ESP_LOGW(TAG, "stage: adc/pwm done");
- `sdkconfig.defaults` +3 −3
- `main/web.c` +2 −0
- `tools/force_sdkconfig.py` +1 −1 — "CONFIG_LOG_DEFAULT_LEVEL_INFO": "y",

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (7).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (6).zip`: экран (1), сайт (1), исходники (1).
- `main/display.c` +41 −4 — #include <stdio.h> / #include "driver/gpio.h"
- `main/web.c` +8 −1
- `main/config.h` +5 −0 — #define CUBE_LCD_BL_GPIO             2 / #define CUBE_LCD_RST_GPIO            (-1)

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (8).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (7).zip`: исходники (2), экран (1).
- `main/display.c` +20 −23 — ESP_LOGW(TAG, "backlight GPIO%d = ON", CUBE_LCD_BL_GPIO); / ESP_LOGW(TAG, "backlight GPIO%d config failed", CUBE_LCD_BL_GPIO);
- `main/config.h` +5 −4 — #define CUBE_LCD_BL_GPIO             6 / #define CUBE_STATUS_LED_GPIO         (-1)
- `sdkconfig.defaults` +1 −1

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (9).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (8).zip`: экран (1), исходники (1).
- `main/display.c` +27 −8 — #include <string.h> / ESP_LOGW(TAG, "backlight GPIO not set (safe); panel-only path");
- `main/config.h` +3 −4 — #define CUBE_LCD_BL_GPIO             (-1)

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (10).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (9).zip`: исходники (2), сайт (1), экран (1).
- `main/web.c` +15 −6 — httpd_resp_set_status(req, "204 No Content"); / ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
- `main/display.c` +14 −4 — ESP_LOGW(TAG, "status LED GPIO%d = ON", CUBE_STATUS_LED_GPIO);
- `main/config.h` +4 −5 — #define CUBE_WEB_KEY                 "FDcube2026" / #define CUBE_LCD_BL_GPIO             20
- `sdkconfig.defaults` +1 −1

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (11).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (10).zip`: исходники (2), экран (1).
- `main/display.c` +18 −40 — #include "esp_lcd_panel_vendor.h" / ESP_LOGW(TAG, "panel create ST7789 240x280...");
- `main/config.h` +6 −3 — #define CUBE_LCD_BL_GPIO             (-1) / #define CUBE_STATUS_LED_GPIO         (-1)
- `sdkconfig.defaults` +1 −1

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (12).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (11).zip`: экран (1), исходники (1).
- `main/display.c` +28 −21 — #include "esp_lcd_ili9341.h" / ESP_LOGW(TAG, "panel create ILI9341 (as original)...");
- `main/config.h` +4 −5 — #define CUBE_LCD_BL_GPIO             2

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (13).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (12).zip`: экран (3), исходники (1), загрузчик и слоты (1).
- `main/display.c` +45 −39 — (void)make_card(scr, 6, 24, 132, 70, "VOLTAGE", &s_voltage); / (void)make_card(scr, 142, 24, 132, 70, "CURRENT", &s_current);
- `main/config.h` +4 −1 — #define CUBE_WEB_KEY                 ""  /* empty = no API password on local AP */ / #define CUBE_LCD_UI_H_RES            280
- `FLASH_RELEASE_OTA1.bat` +2 −2 — python -m esptool --chip esp32c3 --port "%PORT%" -b 921600 read_flash 0x0 0x400000 "%BACKUP_DIR%\cube_before_v2.bin" / python -m esptool --chip esp32c3 --port "%PORT%" -b 921600 write_flash 0x1A0000 "%APP%"
- `FLASH_QUICK.bat` — новый файл
- `QUICK_BUILD.bat` — новый файл

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (14).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (13).zip`: исходники (2), экран (1).
- `main/display.c` +27 −46 — (void)make_card(scr, 8, 28, 108, 72, "VOLTAGE", &s_voltage); / (void)make_card(scr, 124, 28, 108, 72, "CURRENT", &s_current);
- `main/config.h` +0 −3
- `sdkconfig.defaults` +1 −1

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (15).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (14).zip`: экран (2), загрузчик и слоты (1), исходники (1).
- `tools/ota_slot.py` +32 −12 — # after hard_reset from previous op, wait a bit / # Prefer writing a fresh otadata image without relying on a successful read
- `FLASH_QUICK.bat` +14 −5 — python -m esptool --chip esp32c3 --port "%PORT%" -b 921600 --before default_reset --after hard_reset write_flash 0x1A0000 "%APP%"
- `main/display.c` +5 −3
- `main/config.h` +1 −1 — #define CUBE_LCD_SPI_HZ              10000000

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (16).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (15).zip`: экран (1), сайт (1).
- `main/display.c` +12 −9 — ESP_LOGW(TAG, "panel on (gap y=20, mirror X, BGR invert)");
- `main/web.c` +2 −0

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (17).zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (16).zip`: экран (1), исходники (1), сайт (1).
- `main/display.c` +13 −9 — (void)snprintf(pass_line, sizeof(pass_line), "PASS %s", CUBE_WIFI_PASSWORD); / lv_obj_t *hint2 = make_label(scr, "OPEN WiFi (no pass)", 14);
- `main/config.h` +1 −1 — #define CUBE_WIFI_PASSWORD           ""  /* empty = open AP (no password) */
- `main/web.c` +1 −0

## FGMDCharge_FD_ROUND_V2_FIXED_2.0.5-alt.zip

Имя даёт номер 2.0.5. config.h: 2.0.5.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5 (17).zip`: экран (5), прошивка и скрипты (5), исходники (4), загрузчик и слоты (3), сайт (1), документы (1).
- `BUILD_RELEASE.bat` +69 −101 — set "GIT_EXE=" / echo %PROJECT_DIR% | findstr /R "[^ -~]" >nul
- `main/display.c` +37 −81 — (void)make_card(scr, 8, 30, 108, 78, "VOLTAGE", &s_voltage); / (void)make_card(scr, 124, 30, 108, 78, "CURRENT", &s_current);
- `sdkconfig.defaults` +109 −9 — # Size-first build (original app was ~900 KB; keep under 0x190000) / # BLE: only what ble.c needs (legacy Bluedroid GATTS + ADV 4.2)
- `tools/ota_slot.py` +12 −32 — # seq 1 boots OTA0, seq 2 boots OTA1. Mirror to both copies so an older valid copy cannot win.
- `main/web.c` +5 −26
- `main/ble.c` +4 −24
- `RELEASE_FIX_2.0.5.txt` +14 −13
- `main/main.c` +5 −13 — ESP_LOGI(TAG, "FD_ROUND V2 %s running; CAN TX default=%d", CUBE_FW_VERSION,
- `main/config.h` +3 −9 — #define CUBE_WEB_KEY                 "change-me-2026" / #define CUBE_WIFI_PASSWORD           "FDcube2026"  /* change before field use */
- `FLASH_RELEASE_OTA1.bat` +3 −7 — if "%PORT%"=="" exit /b 1 / python -m esptool --chip esp32c3 --port "%PORT%" read-flash 0x0 0x400000 "%BACKUP_DIR%\cube_before_v2.bin"
- и ещё 9 файлов

## FGMDCharge_FD_ROUND_V2_RECONSTRUCTED_2.0.6_FINAL.zip

Имя даёт номер 2.0.6. config.h: 2.0.6.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5-alt.zip`: документы (12), прошивка и скрипты (7), экран (6), исходники (5), загрузчик и слоты (3), сайт (2).
- `main/web_assets.c` +858 −518
- `FILE_INDEX.txt` +106 −104
- `BUILD_RELEASE.bat` +109 −77 — set "PATH_CHECK=%PROJECT_DIR: =%" / if not "%PATH_CHECK%"=="%PROJECT_DIR%" (
- `main/display.c` +93 −41 — #include <stdio.h> / #include <string.h>
- `sdkconfig.defaults` +9 −109 — # Size optimization (OTA slot is 0x190000) / # BLE: required by ble.c (legacy Bluedroid GATTS + ADV 4.2)
- `main/web.c` +87 −7 — httpd_resp_set_status(req, "204 No Content"); / ? "{\"ok\":true,\"tx_enabled\":true,\"tx_compiled\":true,\"web_key_required\":true,\"wifi_auth\":\"WPA2-PSK\"}"
- `RELEASE_MANIFEST_SHA256.txt` +37 −34
- `tools/ota_slot.py` +32 −12 — # after hard_reset from previous op, wait a bit / # Prefer writing a fresh otadata image without relying on a successful read
- `main/ble.c` +24 −4 — ESP_LOGE(TAG, "fallback ADV config failed: %s", esp_err_to_name(err)); / ESP_LOGW(TAG, "ADV using flags-only fallback (name not in packet)");
- `main/main.c` +13 −5 — ESP_LOGW(TAG, "boot %s %s", CUBE_FW_NAME, CUBE_FW_VERSION); / ESP_LOGW(TAG, "stage: adc/pwm done");
- и ещё 25 файлов

## FGMDCharge_FD_ROUND_V2_2.1.0.zip

Имя даёт номер 2.1.0. config.h: 2.1.1.
Относительно `FGMDCharge_FD_ROUND_V2_RECONSTRUCTED_2.0.6_FINAL.zip`: документы (12), исходники (8), экран (3), прошивка и скрипты (3), сайт (2), протокол (1).
- `main/web_assets.c` +512 −390
- `main/app_state.c` +124 −48 — case ST_IDLE: return "IDLE"; / case ST_HANDSHAKE_INIT: return "HANDSHAKE_INIT";
- `README_START_HERE_RU.txt` +49 −66
- `main/app_state.h` +62 −22
- `main/display.c` +50 −13 — #include "esp_lcd_panel_vendor.h" / #include "esp_lcd_panel_ops.h"
- `main/web.c` +39 −7 — #include "sm_relay.h" / "\"can\":%s,\"tx_enabled\":%s,\"rx_count\":%lu,\"tx_count\":%lu,"
- `main/config.h` +29 −12 — #define CUBE_FW_VERSION              "2.1.1" / #define CUBE_WEB_KEY                 "FDcube2026"
- `main/main.c` +22 −2 — #include "sm_relay.h" / if (err != ESP_OK) ESP_LOGE(TAG, "sm_relay init failed: %s", esp_err_to_name(err));
- `main/ble.c` +16 −2 — #include "sm_relay.h"
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.1 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.1
- и ещё 19 файлов

## FGMDCharge_FD_ROUND_V2_2.1.0 (1).zip

Имя даёт номер 2.1.0. config.h: 2.1.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.0.zip`: документы (10), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/display.c` +11 −48 — ESP_LOGW(TAG, "panel on (mirror X=0 Y=0, RGB, gap=0)"); / ESP_LOGW(TAG, "backlight GPIO%d = ON (post-init)", CUBE_LCD_BL_GPIO);
- `README_START_HERE_RU.txt` +3 −9
- `main/web.c` +3 −6 — ESP_LOGW(TAG, "STA disconnected %02X:%02X:%02X:%02X:%02X:%02X AID=%u reason=%u", / {.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_get},
- `main/config.h` +2 −5 — #define CUBE_FW_VERSION              "2.1.0"
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.0 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.0
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.0 / ## Что исправлено в 2.1.0
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.0 / ## 2.1.0 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.1.0 (2).zip

Имя даёт номер 2.1.0. config.h: 2.1.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.0 (1).zip`: экран (1), сайт (1).
- `main/display.c` +11 −5 — #if (CUBE_LCD_BL_GPIO >= 0) && (CUBE_LCD_BL_GPIO != 2) / ESP_LOGW(TAG, "backlight GPIO%d = ON (post-init)", bl_gpio);
- `main/web.c` +3 −2 — ESP_LOGW(TAG, "STA disconnected %02X:%02X:%02X:%02X:%02X:%02X AID=%u",

## FGMDCharge_FD_ROUND_V2_2.1.0-alt.zip

Имя даёт номер 2.1.0. config.h: 2.1.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.0 (2).zip`: документы (9), экран (3), прошивка и скрипты (3), сайт (1), исходники (1).
- `main/CMakeLists.txt` +5 −11 — "settings.c"
- `main/display.c` +5 −11 — ESP_LOGW(TAG, "backlight GPIO%d = ON (post-init)", CUBE_LCD_BL_GPIO);
- `main/web.c` +2 −3 — ESP_LOGW(TAG, "STA disconnected %02X:%02X:%02X:%02X:%02X:%02X AID=%u reason=%u",
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.0.6 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.0.6
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.0.6 / ## Что исправлено в 2.0.6
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.0.6 / ## 2.0.6 — полный инженерный релиз
- `tools/verify_release.py` +2 −2 — VERSION = "2.0.6" / "main/settings.c", "main/settings.h", "main/adc_pwm.c", "main/board_pins.h",
- `tools/static_audit.py` +1 −2 — VERSION = "2.0.6"
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.1.1.zip

Имя даёт номер 2.1.1. config.h: 2.1.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.0-alt.zip`: документы (12), экран (3), прошивка и скрипты (3), исходники (2), сайт (1).
- `main/display.c` +55 −11 — #include "esp_lcd_panel_vendor.h" / #include "esp_lcd_panel_ops.h"
- `main/CMakeLists.txt` +12 −6 — # FD_ROUND V2 application component. / # LCD driver is vendored for deterministic ESP-IDF 5.0.9 builds.
- `README_START_HERE_RU.txt` +14 −3
- `main/web.c` +6 −3 — ESP_LOGW(TAG, "STA disconnected %02X:%02X:%02X:%02X:%02X:%02X AID=%u", / {.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_get},
- `main/config.h` +5 −2 — #define CUBE_FW_VERSION              "2.1.2" / * Many cubes use ST7789 with ILI-compatible branding. Prefer ST7789. */
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.2 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.2
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.2 / ## Что исправлено в 2.1.2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.2 / ## 2.1.2 — полный инженерный релиз
- и ещё 11 файлов

## FGMDCharge_FD_ROUND_V2_2.1.1 (1).zip

Имя даёт номер 2.1.1. config.h: 2.1.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.1.zip`: документы (9), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `README_START_HERE_RU.txt` +4 −9
- `main/display.c` +1 −8
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.1 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.1
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.1 / ## Что исправлено в 2.1.1
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.1 / ## 2.1.1 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `SOURCE_TRUTH_RU.md` +1 −1
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.1.1-alt.zip

Имя даёт номер 2.1.1. config.h: 2.1.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.1 (1).zip`: документы (7), экран (1).
- `README_START_HERE_RU.txt` +3 −9
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.0 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.0
- `FILE_INDEX.txt` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.1.0
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.1.0
- `RELEASE_MANIFEST_SHA256.txt` +0 −1
- `SOURCE_TRUTH_RU.md` +0 −1

## FGMDCharge_FD_ROUND_V2_2.1.2.zip

Текст совпадает с `FGMDCharge_FD_ROUND_V2_2.1.1.zip`. Версия в config.h: 2.1.2.

## FGMDCharge_FD_ROUND_V2_2.1.3.zip

Имя даёт номер 2.1.3. config.h: 2.1.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.1-alt.zip`: документы (10), исходники (4), экран (3), прошивка и скрипты (2), сайт (1).
- `main/adc_pwm.c` +44 −6 — #include "config.h" / #include "driver/ledc.h"
- `main/display.c` +20 −8 — ESP_LOGW(TAG, "panel create ILI9341 240x280..."); / #if CUBE_LCD_USE_ST7789
- `README_START_HERE_RU.txt` +21 −3
- `main/config.h` +10 −5 — #define CUBE_FW_VERSION              "2.1.3" / #define CUBE_WIFI_PASSWORD           ""  /* open AP for max phone visibility */
- `main/web.c` +9 −3 — (void)esp_wifi_set_country_code("01", true); /* world safe */ / ESP_LOGW(TAG, "AP start SSID=%s ch=%u auth=%s tx_power=max",
- `main/main.c` +5 −4 — if (err != ESP_OK) ESP_LOGE(TAG, "Web init failed: %s", esp_err_to_name(err)); / else ESP_LOGW(TAG, "stage: Web AP ok");
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.3 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.3
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.3 / ## Что исправлено в 2.1.3
- и ещё 10 файлов

## FGMDCharge_FD_ROUND_V2_2.1.4.zip

Имя даёт номер 2.1.4. config.h: 2.1.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.3.zip`: документы (9), исходники (6), экран (3), прошивка и скрипты (2), сайт (1).
- `main/display.c` +28 −38 — lv_obj_t *title = make_label(scr, "FGMDCharge", 14); / (void)make_card(scr, 8, 24, 112, 64, "VOLTAGE", &s_voltage);
- `main/adc_pwm.c` +12 −13 — #include "esp_log.h" / #if (CUBE_LCD_BL_PWM_GPIO >= 0)
- `README_START_HERE_RU.txt` +11 −10
- `main/sm_relay.c` +15 −3 — ESP_LOGW(TAG, "relays OK A=GPIO%d B=GPIO%d both OFF", CUBE_RELAY_A_GPIO, CUBE_RELAY_B_GPIO);
- `main/config.h` +6 −7 — #define CUBE_FW_VERSION              "2.1.4" / #define CUBE_WIFI_PASSWORD           ""  /* open AP */
- `main/web.c` +3 −5 — (void)esp_wifi_set_country_code("01", true); / ESP_LOGW(TAG, "AP SSID=%s ch=%u auth=%s",
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.4
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.4 / ## Что исправлено в 2.1.4
- и ещё 11 файлов

## FGMDCharge_FD_ROUND_V2_2.1.5.zip

Имя даёт номер 2.1.5. config.h: 2.1.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.4.zip`: документы (9), экран (8), исходники (3), прошивка и скрипты (2), сайт (1).
- `main/display.c` +36 −18 — #if CUBE_LCD_BGR / #else
- `README_START_HERE_RU.txt` +13 −6
- `main/config.h` +9 −4 — #define CUBE_FW_VERSION              "2.1.5" / #define CUBE_WIFI_SSID               "FGMDCharge"
- `main/web.c` +3 −2 — ESP_LOGI(TAG, "Wi-Fi AP=%s OPEN ch=%u UI=http://192.168.4.1/", CUBE_WIFI_SSID, (unsigned)CUBE_WIFI_CHANNEL);
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.5 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.5
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.5 / ## Что исправлено в 2.1.5
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.5 / ## 2.1.5 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 13 файлов

## FGMDCharge_FD_ROUND_V2_2.1.6.zip

Имя даёт номер 2.1.6. config.h: 2.1.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.5.zip`: документы (9), экран (3), исходники (3), прошивка и скрипты (2), сайт (1).
- `main/display.c` +28 −30 — ESP_LOGW(TAG, "panel create ST7789..."); / ESP_LOGW(TAG, "panel create ILI9341 (2.0.5 path)...");
- `README_START_HERE_RU.txt` +14 −7
- `main/config.h` +13 −8 — #define CUBE_FW_VERSION              "2.1.6" / #define CUBE_WIFI_SSID               "FGMDCharge-V2"
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.6 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.6
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.6 / ## Что исправлено в 2.1.6
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.6 / ## 2.1.6 — полный инженерный релиз
- `main/main.c` +4 −0 — #if (CUBE_BLE_START_DELAY_MS > 0) / ESP_LOGW(TAG, "BLE delay %d ms (SoftAP first)", CUBE_BLE_START_DELAY_MS);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.1.7.zip

Имя даёт номер 2.1.7. config.h: 2.1.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.6.zip`: документы (9), исходники (3), экран (3), прошивка и скрипты (2), сайт (1).
- `main/config.h` +13 −10 — #define CUBE_FW_VERSION              "2.1.7" / #define CUBE_WIFI_SSID               "FGMDCube"
- `README_START_HERE_RU.txt` +8 −8
- `main/web.c` +9 −2 — ESP_LOGW(TAG, "AP MAC %02X:%02X:%02X:%02X:%02X:%02X — scan for SSID '%s' (OPEN)",
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.7 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.7
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.7 / ## Что исправлено в 2.1.7
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.7 / ## 2.1.7 — полный инженерный релиз
- `main/display.c` +2 −2 — ESP_LOGW(TAG, "panel create ST7789 240x280 gap_y..."); / ESP_LOGW(TAG, "panel create ILI9341...");
- `main/main.c` +4 −0 — #if CUBE_BLE_ENABLE / #else
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.1.8.zip

Имя даёт номер 2.1.8. config.h: 2.1.8.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.7.zip`: документы (9), экран (3), исходники (3), прошивка и скрипты (2), сайт (1).
- `main/web_assets.c` +76 −75
- `README_START_HERE_RU.txt` +18 −8
- `main/display.c` +12 −12 — lv_obj_t *title = make_label(scr, "ГБ/Т Зарядка", 14); / (void)make_card(scr, 8, 24, 112, 64, "НАПРЯЖЕНИЕ", &s_voltage);
- `main/app_state.c` +11 −11 — case ST_IDLE: return "ПРОСТОЙ"; / case ST_HANDSHAKE_INIT: return "СВЯЗЬ";
- `main/config.h` +3 −3 — #define CUBE_FW_VERSION              "2.1.8" / #define CUBE_LCD_MIRROR_X            0
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.8 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.8
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.8 / ## Что исправлено в 2.1.8
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.8 / ## 2.1.8 — полный инженерный релиз
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.1.9.zip

Имя даёт номер 2.1.9. config.h: 2.1.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.8.zip`: документы (9), исходники (4), экран (3), сайт (2), прошивка и скрипты (2).
- `main/web_assets.c` +69 −70
- `main/web.c` +11 −16 — ESP_LOGW(TAG, "SoftAP UP SSID=%s OPEN ch=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X", / ESP_LOGW(TAG, "Connect phone to '%s' (no password), open http://192.168.4.1/",
- `README_START_HERE_RU.txt` +16 −9
- `main/display.c` +12 −12 — lv_obj_t *title = make_label(scr, "GB/T CHARGE", 14); / (void)make_card(scr, 8, 24, 112, 64, "VOLTAGE", &s_voltage);
- `main/app_state.c` +11 −11 — case ST_IDLE: return "IDLE"; / case ST_HANDSHAKE_INIT: return "HANDSHAKE";
- `main/config.h` +4 −4 — #define CUBE_FW_VERSION              "2.1.9" / #define CUBE_WEB_KEY                 ""  /* no API key on local AP */
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.9 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.9
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.9 / ## Что исправлено в 2.1.9
- и ещё 10 файлов

## FGMDCharge_FD_ROUND_V2_2.2.0.zip

Имя даёт номер 2.2.0. config.h: 2.2.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.1.9.zip`: документы (10), исходники (7), прошивка и скрипты (4), экран (3).
- `main/display.c` +14 −13 — #include "lv_font_ru_14.h" / lv_obj_t *title = make_label(scr, "ГБ/Т Зарядка", 14);
- `main/app_state.c` +11 −11 — case ST_IDLE: return "ПРОСТОЙ"; / case ST_HANDSHAKE_INIT: return "СВЯЗЬ";
- `README_START_HERE_RU.txt` +10 −10
- `tools/static_audit.py` +9 −7 — VERSION = "2.2.0" / "CONFIG_BT_ENABLED=n", "CONFIG_BT_CONTROLLER_ENABLED=n",
- `main/main.c` +6 −6 — if (err != ESP_OK) ESP_LOGE(TAG, "Web init failed: %s", esp_err_to_name(err)); / else ESP_LOGW(TAG, "stage: Web AP ok");
- `sdkconfig.defaults` +6 −4 — # CONFIG_BT_BLUEDROID_ENABLED is not set / # CONFIG_BT_BLE_ENABLED is not set
- `tools/force_sdkconfig.py` +5 −5 — "CONFIG_BT_ENABLED": "n", / "CONFIG_BT_CONTROLLER_ENABLED": "n",
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.0 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.0
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- и ещё 14 файлов

## FGMDCharge_FD_ROUND_V2_2.2.1.zip

Имя даёт номер 2.2.1. config.h: 2.2.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.0.zip`: документы (9), прошивка и скрипты (3), исходники (3), экран (2).
- `README_START_HERE_RU.txt` +10 −10
- `tools/force_sdkconfig.py` +5 −5 — "CONFIG_BT_ENABLED": "y", / "CONFIG_BT_CONTROLLER_ENABLED": "y",
- `sdkconfig.defaults` +4 −4
- `tools/verify_release.py` +4 −3 — VERSION = "2.2.1" / if "esp_lcd_ili9341" not in cmake:
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.1 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.1
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.1 / ## Что исправлено в 2.2.1
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.1 / ## 2.2.1 — полный инженерный релиз
- `tools/static_audit.py` +2 −2 — VERSION = "2.2.1" / "CONFIG_BT_ENABLED=y", "CONFIG_BT_CONTROLLER_ENABLED=y", "CONFIG_BT_BLUEDROID_ENABLED=y", "CONFIG_BT_BLE_ENABLED=y", "CONFIG_BT_GATTS_ENABLE=y",
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.2.2.zip

Имя даёт номер 2.2.2. config.h: 2.2.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.1.zip`: документы (9), исходники (3), экран (2), прошивка и скрипты (2), сайт (1).
- `README_START_HERE_RU.txt` +10 −10
- `main/main.c` +9 −4 — ESP_LOGW(TAG, "stage: Web AP ok"); / #if (CUBE_WIFI_SCAN_WINDOW_MS > 0)
- `main/web.c` +8 −4 — ESP_LOGW(TAG, "SoftAP UP SSID=%s OPEN ch=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X TX=max", / ESP_LOGW(TAG, ">>> SCAN PHONE NOW for '%s' (no password) http://192.168.4.1/",
- `main/config.h` +6 −3 — #define CUBE_FW_VERSION              "2.2.2" / #define CUBE_BLE_START_DELAY_MS      2000
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.2 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.2
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.2 / ## Что исправлено в 2.2.2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.2 / ## 2.2.2 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.2.3.zip

Имя даёт номер 2.2.3. config.h: 2.2.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.2.zip`: документы (9), исходники (3), сайт (2), экран (2), прошивка и скрипты (2).
- `main/web_assets.c` +74 −73
- `README_START_HERE_RU.txt` +10 −10
- `main/web.c` +14 −6 — .cc = "01", / ESP_LOGW(TAG, "SoftAP %s OPEN ch=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X",
- `main/config.h` +4 −4 — #define CUBE_FW_VERSION              "2.2.3" / #define CUBE_WIFI_CHANNEL            6
- `main/main.c` +1 −7 — else ESP_LOGW(TAG, "stage: Web AP ok");
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.3 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.3
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.3 / ## Что исправлено в 2.2.3
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.3 / ## 2.2.3 — полный инженерный релиз
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.2.4.zip

Имя даёт номер 2.2.4. config.h: 2.2.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.3.zip`: документы (9), исходники (2), экран (2), прошивка и скрипты (2), сайт (1), загрузчик и слоты (1).
- `main/web.c` +32 −5 — (void)esp_netif_str_to_ip4("192.168.4.1", &ip_info.ip); / (void)esp_netif_str_to_ip4("192.168.4.1", &ip_info.gw);
- `README_START_HERE_RU.txt` +11 −11
- `main/config.h` +3 −3 — #define CUBE_FW_VERSION              "2.2.4" / #define CUBE_WEB_KEY                 ""  /* local SoftAP: no API key */
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.4
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.4 / ## Что исправлено в 2.2.4
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.4 / ## 2.2.4 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `FLASH_RELEASE_OTA1.bat` +1 −1
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.2.5.zip

Имя даёт номер 2.2.5. config.h: 2.2.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.4.zip`: документы (9), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/web.c` +46 −72 — (void)snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", CUBE_WIFI_SSID); / "%s", CUBE_WIFI_PASSWORD);
- `README_START_HERE_RU.txt` +10 −10
- `main/config.h` +3 −3 — #define CUBE_FW_VERSION              "2.2.5" / #define CUBE_WIFI_PASSWORD           ""  /* OPEN — official example path */
- `main/display.c` +5 −1 — (void)snprintf(wifi_line, sizeof(wifi_line), "WiFi %s / %s", CUBE_WIFI_SSID, CUBE_WIFI_PASSWORD); / (void)snprintf(wifi_line, sizeof(wifi_line), "WiFi %s OPEN", CUBE_WIFI_SSID);
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.5 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.5
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.5 / ## Что исправлено в 2.2.5
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.5 / ## 2.2.5 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.2.6.zip

Имя даёт номер 2.2.6. config.h: 2.2.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.5.zip`: документы (9), исходники (2), экран (2), прошивка и скрипты (2), загрузчик и слоты (1).
- `README_START_HERE_RU.txt` +10 −10
- `main/config.h` +4 −4 — #define CUBE_FW_VERSION              "2.2.6" / #define CUBE_WIFI_SSID               "Cube"
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.6 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.6
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.6 / ## Что исправлено в 2.2.6
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.6 / ## 2.2.6 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `FLASH_RELEASE_OTA1.bat` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.2.7.zip

Имя даёт номер 2.2.7. config.h: 2.2.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.6.zip`: документы (9), экран (3), сайт (2), исходники (2), прошивка и скрипты (2).
- `main/display.c` +180 −22 — #include "web.h" / #include "esp_wifi.h"
- `README_START_HERE_RU.txt` +10 −10
- `main/web.c` +18 −0 — ESP_LOGW(TAG, "WiFi restart requested"); / ESP_LOGE(TAG, "wifi_stop: %s", esp_err_to_name(err));
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.7 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.7
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.7 / ## Что исправлено в 2.2.7
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.7 / ## 2.2.7 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.2.7 (1).zip

Имя даёт номер 2.2.7. config.h: 2.2.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.7.zip`: экран (1).
- `main/display.c` +2 −2 — #include "sdkconfig.h"

## FGMDCharge_FD_ROUND_V2_2.2.9.zip

Имя даёт номер 2.2.9. config.h: 2.2.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.7 (1).zip`: документы (9), экран (3), прошивка и скрипты (3), сайт (2), исходники (2).
- `main/web.c` +214 −12 — ESP_LOGW(TAG, "auto channel -> %u (load 1=%d 6=%d 11=%d)", best, cnt[1], cnt[6], cnt[11]); / ESP_LOGW(TAG, "STA got IP %s", s_sta_ip);
- `main/display.c` +204 −18 — if (err == ESP_OK) lv_label_set_text(s_sys_status, "WiFi: restarting..."); / else if (err == ESP_ERR_INVALID_STATE) lv_label_set_text(s_sys_status, "WiFi: busy");
- `main/web.h` +30 −0 — #include <stddef.h> / #include <stdint.h>
- `README_START_HERE_RU.txt` +10 −10
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.2.9 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.2.9
- `BUILD_RELEASE.bat` +2 −2
- `FILE_INDEX.txt` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.9 / ## Что исправлено в 2.2.9
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.2.9 / ## 2.2.9 — полный инженерный релиз
- `main/config.h` +2 −2 — #define CUBE_FW_VERSION              "2.2.9" / #define CUBE_WIFI_CHANNEL            0  /* 0 = auto pick 1/6/11 */
- и ещё 9 файлов

## FGMDCharge_FD_ROUND_V2_2.2.9_UI_DIAG_FIXED.zip

Имя даёт номер 2.2.9. config.h: 2.2.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.9.zip`: исходники (7), экран (3), сайт (2), прошивка и скрипты (1), документы (1).
- `main/web_assets.c` +1200 −1373
- `main/display.c` +153 −27 — #include "settings.h" / #include "diag.h"
- `main/web.c` +43 −8 — #include "display.h" / #include "settings.h"
- `main/settings.c` +27 −0 — #define CUBE_NVS_BRIGHTNESS "brightness"
- `main/config.h` +8 −7 — #define CUBE_LCD_BL_GPIO             2 / #define CUBE_LCD_BL_PWM_GPIO         2
- `main/touch_cst816s.c` +3 −3
- `main/display.h` +3 −0
- `main/settings.h` +3 −0 — #include <stdint.h>
- `sdkconfig.defaults` +3 −0
- `tools/force_sdkconfig.py` +3 −0 — "CONFIG_FREERTOS_USE_TRACE_FACILITY": "y", / "CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS": "y",
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.3.0.zip

Имя даёт номер 2.3.0. config.h: 2.3.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.2.9_UI_DIAG_FIXED.zip`: исходники (12), документы (11), экран (10), прошивка и скрипты (5), сайт (3), загрузчик и слоты (1).
- `main/web_assets.c` +799 −1230
- `main/display.c` +395 −756 — #include "app_state.h" / #include "touch_cst816s.h"
- `main/lv_font_ru_14.c` +5 −1066 — * Russian glyphs use LVGL built-in / montserrat where available. / * Delete this file if not referenced. */
- `main/web.c` +72 −316 — online ? "true" : "false", cube_charge_stage_name(s.charger_stage), (unsigned long)s.charger_stage_ms, / ? "{\"ok\":true,\"tx_enabled\":true,\"tx_compiled\":true,\"web_key_required\":true,\"wifi_auth\":\"WPA2-PSK\"}"
- `main/config.h` +50 −52 — #define CUBE_FW_VERSION              "2.3.0" / #define CUBE_WEB_KEY                 ""
- `README_START_HERE_RU.txt` +7 −38
- `main/web.h` +1 −30
- `main/main.c` +14 −15 — if (err != ESP_OK) ESP_LOGE(TAG, "Web init failed: %s", esp_err_to_name(err)); / else ESP_LOGW(TAG, "stage: Web AP ok");
- `main/settings.c` +0 −27
- `main/app_state.c` +11 −11 — case ST_IDLE: return "IDLE"; / case ST_HANDSHAKE_INIT: return "HANDSHAKE_INIT";
- и ещё 32 файлов

## FGMDCharge_FD_ROUND_V2_2.3.0 (1).zip

Имя даёт номер 2.3.0. config.h: 2.3.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.0.zip`: исходники (1).
- `main/config.h` +0 −26

## FGMDCharge_FD_ROUND_V2_2.3.0-alt.zip

Имя даёт номер 2.3.0. config.h: 2.3.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.0 (1).zip`: документы (7), исходники (3), прошивка и скрипты (2), сайт (1), экран (1).
- `main/web_assets.c` +462 −570
- `README_START_HERE_RU.txt` +6 −6
- `main/config.h` +3 −4
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.1.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.1.4
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.4 / ## Что исправлено в 2.1.4
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.1.4 / ## 2.1.4 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.1.4
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.1.4
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.3.1.zip

Имя даёт номер 2.3.1. config.h: 2.3.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.0-alt.zip`: документы (8), сайт (3), экран (3), исходники (3), прошивка и скрипты (2).
- `main/web_assets.c` +568 −460
- `main/display.c` +276 −125 — snprintf(b, sizeof(b), "Bright %d%%", v); / if (s_scan_lbl) lv_label_set_text(s_scan_lbl, "SCAN...");
- `main/web.c` +76 −2 — static char s_sta_ip[20] = "--"; / if (index < 0 || index >= s_scan_n) return "";
- `main/config.h` +31 −4 — #define CUBE_FW_VERSION              "2.3.1" / #define CUBE_HTTP_MAX_UPLOAD       (0x190000)
- `README_START_HERE_RU.txt` +6 −6
- `main/web.h` +5 −0
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.1 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.1
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.1 / ## Что исправлено в 2.3.1
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.1 / ## 2.3.1 — полный инженерный релиз
- и ещё 9 файлов

## FGMDCharge_FD_ROUND_V2_2.3.1 (1).zip

Имя даёт номер 2.3.1. config.h: 2.3.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.1.zip`: экран (1), прошивка и скрипты (1).
- `BUILD_RELEASE.bat` +2 −2
- `tools/verify_release.py` +1 −1 — VERSION = "2.3.0"

## FGMDCharge_FD_ROUND_V2_2.3.1-alt.zip

Имя даёт номер 2.3.1. config.h: 2.3.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.1 (1).zip`: экран (1).
- `main/display.c` +5 −4 — s_br_label = big_label(s_scr_sys, "Bright 70%", 148);

## FGMDCharge_FD_ROUND_V2_2.3.2.zip

Имя даёт номер 2.3.2. config.h: 2.3.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.1-alt.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/display.c` +143 −159 — const char *d = p == 0 ? "o  -  -" : (p == 1 ? "-  o  -" : "-  -  o"); / lv_label_set_text(s_scan_lbl, "nets: 0");
- `main/web.c` +15 −5 — ESP_LOGW(TAG, "background scan start"); / ESP_LOGW(TAG, "background scan done, AP stays up");
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.2 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.2
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.2 / ## Что исправлено в 2.3.2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.2 / ## 2.3.2 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.2
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.3.3.zip

Имя даёт номер 2.3.3. config.h: 2.3.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.2.zip`: документы (7), исходники (5), экран (3), прошивка и скрипты (2), сайт (1).
- `main/web.c` +12 −2 — s.touch_enabled ? "true" : "false", s.touch_pressed ? "true" : "false", / s.ccs_charge_permit ? "YES" : "NO",
- `README_START_HERE_RU.txt` +6 −6
- `main/app_state.c` +9 −0 — if (protocol == 1) return "GBT-2015"; / if (protocol == 2) return "GBT-2011";
- `main/display.c` +2 −3 — line(s_scr_chg, "CUBE  2.3.3", 4); / snprintf(b, sizeof(b), "%s %s", cube_protocol_name(st.protocol), cube_main_state_name(st.main_state));
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.3 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.3
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.3 / ## Что исправлено в 2.3.3
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.3 / ## 2.3.3 — полный инженерный релиз
- `main/settings.h` +1 −3 — #include "config.h"
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.3.4.zip

Имя даёт номер 2.3.4. config.h: 2.3.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.3.zip`: документы (6), исходники (3), экран (2), прошивка и скрипты (2), сайт (1).
- `main/app_state.c` +41 −1 — static char s_vin[18] = "UNKNOWNVIN000000"; / snprintf(s_log[i], sizeof(s_log[i]), "%lu %s", (unsigned long)now_ms(), msg);
- `main/web.c` +35 −0 — httpd_resp_set_type(req, "text/plain; charset=utf-8"); / sscanf(body, "v=%d&i=%d&soc=%d&vin=%17s", &v, &i, &soc, vin);
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.4
- `BUILD_RELEASE.bat` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.4 / ## 2.3.4 — полный инженерный релиз
- `main/app_state.h` +4 −0
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.4
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.3.5.zip

Имя даёт номер 2.3.5. config.h: 2.3.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.4.zip`: документы (6), исходники (2), экран (2), прошивка и скрипты (2).
- `main/sm_relay.c` +76 −10 — #include <string.h> / memcpy(&b[24], "TESTVIN000000001", 17);
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.5 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.5
- `BUILD_RELEASE.bat` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.5 / ## 2.3.5 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.5
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.3.5
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.3.5"
- и ещё 2 файлов

## FGMDCharge_FD_ROUND_V2_2.3.6.zip

Имя даёт номер 2.3.6. config.h: 2.3.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.5.zip`: документы (6), исходники (3), экран (2), прошивка и скрипты (2).
- `README_START_HERE_RU.txt` +6 −6
- `main/app_state.c` +9 −0 — snprintf(m, sizeof(m), "RX id 0x%08lX dlc %u", (unsigned long)f->id, f->dlc);
- `main/sm_relay.c` +7 −2 — app_state_log("TX BRM 41B GBT-2011"); / app_state_log("TX BRM 49B GBT-2015");
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.6 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.6
- `BUILD_RELEASE.bat` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.6 / ## 2.3.6 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.6
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.3.6
- и ещё 3 файлов

## FGMDCharge_FD_ROUND_V2_2.3.7.zip

Имя даёт номер 2.3.7. config.h: 2.3.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.6.zip`: документы (6), исходники (2), экран (2), прошивка и скрипты (2).
- `main/sm_relay.c` +26 −9 — app_state_log("TX BEM timeout, relays off"); / app_state_log("TX BSD end");
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.7 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.7
- `BUILD_RELEASE.bat` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.7 / ## 2.3.7 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.7
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.3.7
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.3.7"
- и ещё 2 файлов

## FGMDCharge_FD_ROUND_V2_2.3.8.zip

Имя даёт номер 2.3.8. config.h: 2.3.8.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.7.zip`: документы (6), экран (2), прошивка и скрипты (2), исходники (1).
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.8 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.8
- `BUILD_RELEASE.bat` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.8 / ## 2.3.8 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.8
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.3.8
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.3.8"
- `tools/static_audit.py` +1 −1 — VERSION = "2.3.8"
- и ещё 1 файлов

## FGMDCharge_FD_ROUND_V2_2.3.9.zip

Имя даёт номер 2.3.9. config.h: 2.3.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.8.zip`: документы (6), исходники (2), экран (2), прошивка и скрипты (2).
- `main/sm_relay.c` +23 −1 — app_state_log("auto start: station on CAN"); / app_state_log("unplug: CAN silent, relays off");
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.3.9 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.3.9
- `BUILD_RELEASE.bat` +2 −2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.9 / ## 2.3.9 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.3.9
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.3.9
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.3.9"
- и ещё 2 файлов

## FGMDCharge_FD_ROUND_V2_2.4.0.zip

Имя даёт номер 2.4.0. config.h: 2.4.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.3.9.zip`: документы (7), исходники (5), экран (3), прошивка и скрипты (2), сайт (1).
- `main/sm_relay.c` +38 −0 — app_state_log("relay test GPIO6 ON"); / app_state_log("12V up, handshake now");
- `main/display.c` +17 −1 — #include "sm_relay.h" / lv_label_set_text(tl, "TEST RELAY");
- `README_START_HERE_RU.txt` +6 −6
- `main/app_state.c` +0 −5
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.0 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.0
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.0 / ## Что исправлено в 2.4.0
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.0 / ## 2.4.0 — полный инженерный релиз
- `main/web.c` +3 −1 — "\"ble\":%s,\"notify\":%s,\"indicate\":%s,\"display\":%s,\"touch\":%s,\"touch_pressed\":%s," / "\"protocol\":\"%s\",\"voltage_v\":%.1f,\"current_a\":%.1f,\"power_w\":%ld,\"time_min\":%u,"
- `main/sm_relay.h` +2 −1
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.4.0 (1).zip

Имя даёт номер 2.4.0. config.h: 2.4.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.0.zip`: исходники (2), сайт (1).
- `main/sm_relay.c` +0 −9
- `main/app_state.c` +5 −0
- `main/web.c` +1 −3

## FGMDCharge_FD_ROUND_V2_2.4.0 (2).zip

Имя даёт номер 2.4.0. config.h: 2.4.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.0 (1).zip`: исходники (1).
- `main/app_state.c` +0 −5

## FGMDCharge_FD_ROUND_V2_2.4.0 (3).zip

Имя даёт номер 2.4.0. config.h: 2.4.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.0 (2).zip`: сайт (1).
- `main/web.c` +3 −1 — "\"ble\":%s,\"notify\":%s,\"indicate\":%s,\"display\":%s,\"touch\":%s,\"touch_pressed":%s," / "\"protocol\":\"%s\",\"voltage_v\":%.1f,\"current_a\":%.1f,\"power_w\":%ld,\"time_min\":%u,"

## FGMDCharge_FD_ROUND_V2_2.4.0-alt.zip

Имя даёт номер 2.4.0. config.h: 2.4.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.0 (3).zip`: исходники (2), документы (1), сайт (1).
- `main/app_state.c` +5 −0
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.3.3 / ## Что исправлено в 2.3.3
- `main/web.c` +1 −3
- `main/idf_component.yml` +1 −1 — version: "2.3.3"

## FGMDCharge_FD_ROUND_V2_2.4.1.zip

Имя даёт номер 2.4.1. config.h: 2.4.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.0-alt.zip`: документы (7), исходники (5), экран (3), прошивка и скрипты (2), сайт (1).
- `main/display.c` +21 −24 — line(s_scr_chg, "CUBE 2.4.1", 4); / s_card_u = card(s_scr_chg, 10, 28, &s_voltage, "V");
- `main/app_state.c` +23 −10
- `main/sm_relay.c` +24 −4 — app_state_log("load GPIO7 ON, 3s after station power");
- `README_START_HERE_RU.txt` +6 −6
- `main/web.c` +5 −1 — "\"ble\":%s,\"notify\":%s,\"indicate\":%s,\"display\":%s,\"touch\":%s,\"touch_pressed\":%s," / "\"protocol\":\"%s\",\"voltage_v\":%.1f,\"current_a\":%.1f,\"power_w\":%ld,\"time_min\":%u,"
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.1 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.1
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.1 / ## Что исправлено в 2.4.1
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.1 / ## 2.4.1 — полный инженерный релиз
- `main/app_state.h` +4 −0
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.4.1-alt.zip

Имя даёт номер 2.4.1. config.h: 2.4.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.1.zip`: исходники (1).
- `main/app_state.c` +0 −2

## FGMDCharge_FD_ROUND_V2_2.4.2.zip

Имя даёт номер 2.4.2. config.h: 2.4.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.1-alt.zip`: документы (7), экран (3), исходники (3), прошивка и скрипты (2).
- `main/display.c` +19 −2 — line(s_scr_chg, "CUBE 2.4.2", 4); / lv_label_set_text(sl, "HOLD STOP");
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.2 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.2
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.2 / ## Что исправлено в 2.4.2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.2 / ## 2.4.2 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.4.2
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.4.2
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.4.3.zip

Имя даёт номер 2.4.3. config.h: 2.4.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.2.zip`: документы (7), экран (3), исходники (3), прошивка и скрипты (2).
- `main/display.c` +28 −22 — line(s_scr_chg, "CUBE 2.4.3", 6); / s_card_u = card(s_scr_chg, 10, 26, &s_voltage, "V");
- `README_START_HERE_RU.txt` +6 −6
- `main/sm_relay.c` +6 −2 — app_state_log("load held: request below station min");
- `main/config.h` +3 −3 — #define CUBE_FW_VERSION              "2.4.3" / #define CUBE_DEFAULT_REQ_VOLTAGE_0P1V    2400
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.3 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.3
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.3 / ## Что исправлено в 2.4.3
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.3 / ## 2.4.3 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.4.4.zip

Имя даёт номер 2.4.4. config.h: 2.4.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.3.zip`: документы (7), исходники (3), экран (3), прошивка и скрипты (2).
- `README_START_HERE_RU.txt` +6 −6
- `main/sm_relay.c` +2 −5
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.4
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.4 / ## Что исправлено в 2.4.4
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.4 / ## 2.4.4 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.4.4
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.4.4
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.4.5.zip

Имя даёт номер 2.4.5. config.h: 2.4.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.4.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2).
- `README_START_HERE_RU.txt` +6 −6
- `main/display.c` +4 −4 — line(s_scr_chg, "CUBE 2.4.5", 6); / lv_label_set_text(stop_lbl, "HOLD STOP");
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.5 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.5
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.5 / ## Что исправлено в 2.4.5
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.5 / ## 2.4.5 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.4.5
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.4.5
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.4.6.zip

Имя даёт номер 2.4.6. config.h: 2.4.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.5.zip`: документы (7), экран (3), сайт (2), исходники (2), прошивка и скрипты (2).
- `main/display.c` +20 −17 — line(s_scr_chg, "CUBE 2.4.6", 6); / s_card_u = card(s_scr_chg, 10, 40, &s_voltage, "V");
- `main/web.c` +28 −3 — s.bcl_mode ? "CC" : "CV", CUBE_WIFI_SSID, (unsigned)s.substate, / appendf(buf, sizeof(buf), &used, "{\"ok\":true,\"nets\":[");
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.6 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.6
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.6 / ## Что исправлено в 2.4.6
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.6 / ## 2.4.6 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.4.6
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.4.7.zip

Имя даёт номер 2.4.7. config.h: 2.4.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.6.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.7 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.7
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.7 / ## Что исправлено в 2.4.7
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.7 / ## 2.4.7 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.4.7
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.4.7
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.4.7"
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.4.8.zip

Имя даёт номер 2.4.8. config.h: 2.4.8.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.7.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1), загрузчик и слоты (1).
- `main/web.c` +34 −0 — #include "esp_ota_ops.h" / #include "esp_partition.h"
- `main/display.c` +12 −3 — line(s_scr_chg, "CUBE 2.4.8", 22); / lv_textarea_set_placeholder_text(s_wifi_ta, "password");
- `README_START_HERE_RU.txt` +6 −6
- `main/safe_ota.c` +7 −1 — #include "esp_partition.h"
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.8 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.8
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.8 / ## Что исправлено в 2.4.8
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.8 / ## 2.4.8 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.4.9.zip

Имя даёт номер 2.4.9. config.h: 2.4.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.8.zip`: документы (7), исходники (3), экран (3), загрузчик и слоты (1).
- `main/main.c` +37 −0 — #include "esp_ota_ops.h" / #include "nvs.h"
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.4.9 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.4.9
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.9 / ## Что исправлено в 2.4.9
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.4.9 / ## 2.4.9 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.4.9
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.4.9
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.5.0.zip

Имя даёт номер 2.5.0. config.h: 2.5.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.4.9.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2).
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.0 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.0
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.0 / ## Что исправлено в 2.5.0
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.0 / ## 2.5.0 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.0
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.5.0
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.5.0"
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.5.1.zip

Имя даёт номер 2.5.1. config.h: 2.5.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.0.zip`: документы (7), загрузчик и слоты (6), экран (3), исходники (2), прошивка и скрипты (2).
- `FLASH_ORIGINAL_OTA0.bat` +37 −4 — cd /d "%~dp0" / set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.1 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.1
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.1 / ## Что исправлено в 2.5.1
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.1 / ## 2.5.1 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.1
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.5.1
- и ещё 10 файлов

## FGMDCharge_FD_ROUND_V2_2.5.2.zip

Имя даёт номер 2.5.2. config.h: 2.5.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.1.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (2).
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.2 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.2
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.2 / ## Что исправлено в 2.5.2
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.2 / ## 2.5.2 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.2
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.5.2
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.5.2"
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.5.3.zip

Имя даёт номер 2.5.3. config.h: 2.5.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.2.zip`: документы (7), исходники (3), экран (3), прошивка и скрипты (3), загрузчик и слоты (2).
- `main/main.c` +0 −33
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.3 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.3
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.3 / ## Что исправлено в 2.5.3
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.3 / ## 2.5.3 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.3
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.5.3
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.5.3 (1).zip

Имя даёт номер 2.5.3. config.h: 2.5.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.3.zip`: загрузчик и слоты (2).
- `bootloader_components/slot_select/hooks.c` +17 −15 — #include "bootloader_flash.h"
- `bootloader_components/slot_select/CMakeLists.txt` +1 −1 — idf_component_register(SRCS "hooks.c" REQUIRES bootloader_support)

## FGMDCharge_FD_ROUND_V2_2.5.3 (2).zip

Имя даёт номер 2.5.3. config.h: 2.5.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.3 (1).zip`: загрузчик и слоты (1).
- `bootloader_components/slot_select/CMakeLists.txt` +1 −1 — idf_component_register(SRCS "hooks.c" PRIV_REQUIRES bootloader_support spi_flash)

## FGMDCharge_FD_ROUND_V2_2.5.3-alt.zip

Имя даёт номер 2.5.3. config.h: 2.5.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.3 (2).zip`: загрузчик и слоты (2), прошивка и скрипты (1).
- `bootloader_components/slot_select/hooks.c` +10 −8 — #define COUNT_MAX 3
- `bootloader_components/slot_select/CMakeLists.txt` +1 −1 — idf_component_register(SRCS "hooks.c" REQUIRES bootloader_support)
- `GO.bat` — файл убран

## FGMDCharge_FD_ROUND_V2_2.5.4.zip

Имя даёт номер 2.5.4. config.h: 2.5.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.3-alt.zip`: документы (7), экран (3), прошивка и скрипты (3), загрузчик и слоты (2), исходники (2).
- `bootloader_components/slot_select/hooks.c` +20 −22 — #include "esp_rom_spiflash.h" / #include "esp_rom_sys.h"
- `main/display.c` +27 −3 — const char *nm[] = {"CHG", "SYS", "WIFI"}; / line(s_scr_chg, "CUBE 2.5.4", 22);
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.4 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.4
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.4 / ## Что исправлено в 2.5.4
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.4 / ## 2.5.4 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `bootloader_components/slot_select/CMakeLists.txt` +1 −1 — idf_component_register(SRCS "hooks.c" PRIV_REQUIRES bootloader_support spi_flash)
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.5.5.zip

Имя даёт номер 2.5.5. config.h: 2.5.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.4.zip`: документы (7), экран (3), прошивка и скрипты (3), исходники (3), загрузчик и слоты (1), сайт (1).
- `main/display.c` +25 −37 — if (s_scan_lbl) lv_label_set_text(s_scan_lbl, n ? "tap a network" : "nets: 0"); / line(s_scr_chg, "CUBE 2.5.5", 22);
- `bootloader_components/slot_select/hooks.c` +17 −16 — #include "esp_rom_sys.h" / #include "bootloader_flash_priv.h"
- `BUILD_RELEASE.bat` +12 −6
- `README_START_HERE_RU.txt` +6 −6
- `GO.bat` +5 −0
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.5 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.5
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.5 / ## Что исправлено в 2.5.5
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.5 / ## 2.5.5 — полный инженерный релиз
- `main/web.c` +3 −1 — if (strstr(body, "relay_test")) sm_request_relay_test(); / int o = snprintf(buf, sizeof(buf), "VIN %s\nSTA %s\n", app_state_vin(), web_sta_ip());
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- и ещё 8 файлов

## FGMDCharge_FD_ROUND_V2_2.5.5-alt.zip

Имя даёт номер 2.5.5. config.h: 2.5.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.5.zip`: экран (1), исходники (1).
- `main/display.c` +4 −0
- `main/app_state.h` +0 −2

## FGMDCharge_FD_ROUND_V2_2.5.6.zip

Имя даёт номер 2.5.6. config.h: 2.5.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.5-alt.zip`: документы (7), экран (3), исходники (3), прошивка и скрипты (2), сайт (1).
- `README_START_HERE_RU.txt` +6 −6
- `main/web.c` +11 −0 — httpd_resp_set_type(req, "text/plain; charset=utf-8"); / return httpd_resp_sendstr(req, "URL saved on page. Flash the file from the phone button in this build; Windows SMB is not in the cube yet.");
- `main/display.c` +3 −6 — line(s_scr_chg, "CUBE 2.5.6", 22);
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.6 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.6
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.6 / ## Что исправлено в 2.5.6
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.6 / ## 2.5.6 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.6
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.5.7.zip

Имя даёт номер 2.5.7. config.h: 2.5.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.6.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/display.c` +53 −13 — if (s_dots) lv_label_set_text(s_dots, "home ses set net log sys"); / line(s_scr_chg, "ДОМ 2.5.7", 8);
- `README_START_HERE_RU.txt` +6 −6
- `AUDIT_REPORT_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.7 / ## Что исправлено после лога 2.0.3 / передаётся в сборку 2.5.7
- `BUILD_RELEASE.bat` +2 −2
- `README_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.7 / ## Что исправлено в 2.5.7
- `RELEASE_NOTES_RU.md` +2 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.5.7 / ## 2.5.7 — полный инженерный релиз
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.7
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.5.7
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.5.8.zip

Имя даёт номер 2.5.8. config.h: 2.5.8.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.7.zip`: документы (8), исходники (7), экран (4), сайт (3), загрузчик и слоты (2), прошивка и скрипты (2).
- `main/web_assets.c` +799 −820
- `main/lv_font_ru_14.c` +1449 −5 — * Size: 14 px / * Bpp: 4
- `main/display.c` +260 −57 — #include "safe_ota.h" / #include "can_gbt.h"
- `main/web.c` +151 −17 — #include "display.h" / #include "esp_http_client.h"
- `main/app_state.c` +35 −5 — snprintf(s_vin, sizeof(s_vin), "%s", tmp[0] ? tmp : "UNKNOWNVIN000000"); / app_state_log(protocol == 1 ? "protocol GBT-2015" : protocol == 2 ? "protocol GBT-2011" : "protocol AUTO");
- `main/safe_ota.c` +23 −0 — #include "display.h" / ota_ui(0, "OTA");
- `main/sm_relay.c` +6 −2 — memcpy(&b[24], "00000000000000000", 17);
- `BUILD_RELEASE.bat` +2 −2
- `main/app_state.h` +3 −0
- `AUDIT_REPORT_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.8
- и ещё 16 файлов

## FGMDCharge_FD_ROUND_V2_2.5.9.zip

Имя даёт номер 2.5.9. config.h: 2.5.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.8.zip`: документы (7), экран (3), сайт (2), исходники (2), прошивка и скрипты (2).
- `main/web.c` +20 −15 — destination that cannot hold "http://" plus the whole tail. */ / return httpd_resp_sendstr(req, "url too long");
- `RELEASE_NOTES_RU.md` +13 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.5.9 / ## 2.5.9 — сборка снова проходит
- `AUDIT_REPORT_RU.md` +8 −1 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.5.9 / ## 2.5.9 build fix
- `BUILD_RELEASE.bat` +2 −2
- `main/display.c` +2 −2 — line(s_scr_chg, "ДОМ 2.5.9", 8); / line(s_scr_sys, "СИСТЕМА 2.5.9", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.5.9
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.5.9
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.6.0.zip

Имя даёт номер 2.6.0. config.h: 2.6.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.5.9.zip`: документы (7), экран (3), исходники (3), прошивка и скрипты (3), сайт (1).
- `RELEASE_NOTES_RU.md` +12 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.6.0 / ## 2.6.0 — экран больше не вешает main
- `main/display.c` +12 −2 — ESP_LOGE(TAG, "LVGL OOM pad key %d", i); / ESP_LOGE(TAG, "LVGL OOM label '%s'", txt ? txt : "");
- `AUDIT_REPORT_RU.md` +7 −2 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.6.0 / ## 2.6.0
- `BUILD_RELEASE.bat` +2 −2
- `sdkconfig.defaults` +2 −1 — # CONFIG_LV_USE_ASSERT_MALLOC is not set
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.6.0
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.6.0
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.7.0.zip

Имя даёт номер 2.7.0. config.h: 2.7.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.6.0.zip`: документы (7), исходники (4), экран (3), сайт (2), прошивка и скрипты (2), загрузчик и слоты (1).
- `main/display.c` +225 −93 — if (s_scan_lbl) lv_label_set_text(s_scan_lbl, "SCAN..."); / if (strcmp(k, "C") == 0) { lv_textarea_set_text(ta, ""); return; }
- `main/web.c` +31 −13 — ESP_LOGW(TAG, "scan[%d] %s rssi=%d ch=%u", i, s_scan_ssid[i], rec[i].rssi, rec[i].primary);
- `main/main.c` +23 −5 — #include "esp_log.h" / #include <stdio.h>
- `RELEASE_NOTES_RU.md` +14 −3 — # FGMDCharge / FD_ROUND V2 — Release 2.7.0 / ## 2.7.0 — поля, список сетей, край экрана, точка доступа
- `bootloader_components/slot_select/hooks.c` +11 −6 — #include "soc/rtc_cntl_reg.h" / esp_rom_printf("BOOT reason %u marks %08x %08x %08x slot %d\n",
- `AUDIT_REPORT_RU.md` +6 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.0 / ## 2.7.0
- `main/app_state.c` +4 −4 — o += snprintf(out + o, (size_t)(out_len - o), "%s\n", s_log[n % 20]);
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.0
- и ещё 9 файлов

## FGMDCharge_FD_ROUND_V2_2.7.1.zip

Имя даёт номер 2.7.1. config.h: 2.7.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.0.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `RELEASE_NOTES_RU.md` +9 −4 — # FGMDCharge / FD_ROUND V2 — Release 2.7.1 / ## 2.7.1 — сборка
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.1 / ## 2.7.1
- `main/display.c` +3 −3 — line(s_scr_chg, "ДОМ 2.7.1", 8); / line(s_scr_sys, "СИСТЕМА 2.7.1", 4);
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.1
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.1
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.7.1
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.7.2.zip

Имя даёт номер 2.7.2. config.h: 2.7.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.1.zip`: документы (7), экран (3), сайт (2), исходники (2), прошивка и скрипты (2).
- `main/display.c` +21 −13 — if (strcmp(k, "<") == 0) { lv_textarea_del_char(ta); return; } / const char *keys[] = {"1","2","3","4","5","6","7","8","9","<","0","OK"};
- `main/web.c` +7 −25 — ESP_LOGI(TAG, "scan done n=%d", s_scan_n);
- `RELEASE_NOTES_RU.md` +10 −5 — # FGMDCharge / FD_ROUND V2 — Release 2.7.2 / ## 2.7.2
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.2 / ## 2.7.2
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.2
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.2
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.7.3.zip

Имя даёт номер 2.7.3. config.h: 2.7.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.2.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `RELEASE_NOTES_RU.md` +11 −6 — # FGMDCharge / FD_ROUND V2 — Release 2.7.3 / ## 2.7.3 — сборка
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.3 / ## 2.7.3
- `main/display.c` +3 −3 — line(s_scr_chg, "ДОМ 2.7.3", 8); / line(s_scr_sys, "СИСТЕМА 2.7.3", 4);
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.3
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.3
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.7.3
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.7.4.zip

Имя даёт номер 2.7.4. config.h: 2.7.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.3.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1), сайт (1).
- `main/display.c` +38 −38 — line(s_scr_chg, "ДОМ 2.7.4", 8); / s_dots = line(s_scr_chg, "листай вниз", 248);
- `RELEASE_NOTES_RU.md` +12 −7 — # FGMDCharge / FD_ROUND V2 — Release 2.7.4 / ## 2.7.4
- `bootloader_components/slot_select/hooks.c` +10 −5 — #include "bootloader_utility.h" / esp_rom_printf("BOOT switch to slot %d, reset now\n", next);
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.4 / ## 2.7.4
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.4
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.4
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_FIXED.zip

Имя даёт номер без номера. config.h: 2.7.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.4.zip`: документы (4), загрузчик и слоты (2), исходники (1), экран (1).
- `main/config.h` +14 −3 — * over UART from this exact board) says "ili9341: Install ILI9341 panel / * driver" — the panel is ILI9341, not ST7789. If the display shows
- `analysis_original/FIRMWARE_REPORT_RU.md` — файл убран
- `analysis_original/KANONICAL_VALUES.md` — файл убран
- `analysis_original/RECOVERED_HARDWARE.md` — файл убран
- `analysis_original/REVERSE_ENGINEERING_STATUS.md` — файл убран
- `analysis_original/ota0_segments.txt` — файл убран
- `analysis_original/ota1_segments.txt` — файл убран
- `analysis_original/quick_check.py` — файл убран

## FGMDCharge_FD_ROUND_V2grok.zip

Имя даёт номер без номера. config.h: 2.7.4.
Относительно `FGMDCharge_FD_ROUND_V2_FIXED.zip`: исходники (1).
- `main/config.h` +3 −14 — #define CUBE_LCD_USE_ST7789          1 / #define CUBE_LCD_V_RES               280

## FGMDCharge_FD_ROUND_V2_2.7.5.zip

Имя даёт номер 2.7.5. config.h: 2.7.5.
Относительно `FGMDCharge_FD_ROUND_V2grok.zip`: экран (1), исходники (1).
- `main/display.c` +75 −7 — if (s_pass_title) lv_label_set_text(s_pass_title, "пароль"); / if (s_wifi_ta) lv_textarea_set_text(s_wifi_ta, "");
- `main/config.h` +1 −1 — #define CUBE_FW_VERSION              "2.7.5"

## FGMDCharge_FD_ROUND_V2_2.7.6.zip

Имя даёт номер 2.7.6. config.h: 2.7.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.5.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2).
- `main/display.c` +28 −14 — lv_label_set_text(bt, "4 цифры"); / lv_label_set_text(l, keys[i][0] == 'O' ? "V" : keys[i]);
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.7.6 / ## 2.7.6
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.6 / ## 2.7.6
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.6
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.6
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.7.6
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.7.6 (1).zip

Имя даёт номер 2.7.6. config.h: 2.7.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.6.zip`: экран (1), прошивка и скрипты (1).
- `main/display.c` +3 −4
- `tools/verify_release.py` +1 −1 — VERSION = "2.7.4"

## FGMDCharge_FD_ROUND_V2_2.7.6 (2).zip

Имя даёт номер 2.7.6. config.h: 2.7.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.6 (1).zip`: прошивка и скрипты (1).
- `tools/verify_release.py` +1 −1 — VERSION = "2.7.6"

## FGMDCharge_FD_ROUND_V2_2.7.6 (3).zip

Имя даёт номер 2.7.6. config.h: 2.7.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.6 (2).zip`: экран (1).
- `main/display.c` +1 −0

## FGMDCharge_FD_ROUND_V2_2.7.6-alt.zip

Имя даёт номер 2.7.6. config.h: 2.7.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.6 (3).zip`: документы (7), экран (3), прошивка и скрипты (2), исходники (1).
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.7.4 / ## 2.7.4
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.4 / ## 2.7.4
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.4
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.4
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.7.4
- `main/idf_component.yml` +1 −1 — version: "2.7.4"
- и ещё 3 файлов

## FGMDCharge_FD_ROUND_V2_2.7.7.zip

Имя даёт номер 2.7.7. config.h: 2.7.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.6-alt.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/display.c` +21 −28 — line(s_scr_chg, "КУБ 2.7.7", 4); / s_dots = line(s_scr_chg, "ниже: точка, лог, оригинал", 250);
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.7.7 / ## 2.7.7
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.7 / ## 2.7.7
- `main/web.c` +3 −3 — ESP_LOGW(TAG, "AP only SSID=%s ch=%u",
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.7
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.7.7
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.7.7-alt.zip

Имя даёт номер 2.7.7. config.h: 2.7.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.7.zip`: экран (1).
- `main/display.c` +7 −7 — lv_label_set_text(obl, "оригинал");

## FGMDCharge_FD_ROUND_V2_2.7.8.zip

Имя даёт номер 2.7.8. config.h: 2.7.8.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.7-alt.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/display.c` +9 −10 — line(s_scr_chg, "КУБ 2.7.8", 4); / lv_label_set_text(orig_l, "оригинал");
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.7.8 / ## 2.7.8
- `main/web.c` +7 −2 — .cc = "01", .schan = 1, .nchan = 13, .policy = WIFI_COUNTRY_POLICY_MANUAL
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.8 / ## 2.7.8
- `BUILD_RELEASE.bat` +2 −2
- `main/config.h` +2 −2 — #define CUBE_FW_VERSION              "2.7.8" / #define CUBE_WIFI_CHANNEL            1
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.8
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.7.9.zip

Имя даёт номер 2.7.9. config.h: 2.7.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.8.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1), сайт (1).
- `main/display.c` +30 −5 — line(s_scr_chg, "КУБ 2.7.9", 4); / s_status = line(s_scr_chg, "питание = выбор прошивки", 476);
- `bootloader_components/slot_select/hooks.c` +7 −18 — esp_rom_printf("BOOT reason %u slot %d\n", (unsigned)reason, slot); / esp_rom_printf("BOOT power-on -> our menu, reset\n");
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.7.9 / ## 2.7.9
- `main/web.c` +2 −9
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.7.9 / ## 2.7.9
- `BUILD_RELEASE.bat` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.7.9
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.7.9-alt.zip

Имя даёт номер 2.7.9. config.h: 2.7.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.9.zip`: загрузчик и слоты (1).
- `bootloader_components/slot_select/hooks.c` +0 −2

## FGMDCharge_FD_ROUND_V2_2.8.0.zip

Имя даёт номер 2.8.0. config.h: 2.8.0.
Относительно `FGMDCharge_FD_ROUND_V2_2.7.9-alt.zip`: документы (7), экран (3), загрузчик и слоты (2), исходники (2), прошивка и скрипты (2), сайт (1).
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.0 / ## 2.8.0
- `bootloader_components/slot_select/hooks.c` +11 −5 — #define CHOICE ((volatile uint32_t *)0x50001FF0) / #define MAGIC_OLD 0x01D0F00Du
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.0 / ## 2.8.0
- `BUILD_RELEASE.bat` +2 −2
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.0", 4); / line(s_scr_sys, "СИСТЕМА 2.8.0", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.0
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.8.0
- и ещё 7 файлов

## FGMDCharge_FD_ROUND_V2_2.8.1.zip

Имя даёт номер 2.8.1. config.h: 2.8.1.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.0.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1).
- `bootloader_components/slot_select/hooks.c` +36 −11 — *seq_out = seq; / *slot_out = slot;
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.1 / ## 2.8.1
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.1 / ## 2.8.1
- `BUILD_RELEASE.bat` +2 −2
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.1", 4); / line(s_scr_sys, "СИСТЕМА 2.8.1", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.1
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.8.1
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.8.2.zip

Имя даёт номер 2.8.2. config.h: 2.8.2.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.1.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1), сайт (1).
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.2 / ## 2.8.2
- `bootloader_components/slot_select/hooks.c` +11 −4 — #define MAGIC_FORCED 0x0F0C3D01u / esp_rom_printf("BOOT menu already forced\n");
- `main/web.c` +4 −9 — (void)snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", "12345678"); / ESP_LOGW(TAG, "AP only SSID=%s ch=%u key=12345678",
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.2 / ## 2.8.2
- `BUILD_RELEASE.bat` +2 −2
- `main/config.h` +2 −2 — #define CUBE_FW_VERSION              "2.8.2" / #define CUBE_WIFI_CHANNEL            6
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.2", 4); / line(s_scr_sys, "СИСТЕМА 2.8.2", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.2
- `README_START_HERE_RU.txt` +1 −1
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.8.3.zip

Имя даёт номер 2.8.3. config.h: 2.8.3.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.2.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1), сайт (1).
- `bootloader_components/slot_select/hooks.c` +15 −21 — #define STORE0  0x600080B0u / esp_rom_printf("BOOT wrote seq %u slot %d both sectors\n", (unsigned)seq, slot);
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.3 / ## 2.8.3
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.3 / ## 2.8.3
- `main/web.c` +3 −2 — #include "soc/rtc_cntl_reg.h" / ESP_LOGW(TAG, "softAP started SSID:%s password:12345678 channel:%d", CUBE_WIFI_SSID, s_ap_channel);
- `BUILD_RELEASE.bat` +2 −2
- `main/config.h` +2 −2 — #define CUBE_FW_VERSION              "2.8.3" / #define CUBE_WIFI_PASSWORD           "12345678"
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.3", 4); / line(s_scr_sys, "СИСТЕМА 2.8.3", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.3
- `README_START_HERE_RU.txt` +1 −1
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.8.4.zip

Имя даёт номер 2.8.4. config.h: 2.8.4.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.3.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1), сайт (1).
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.4 / ## 2.8.4
- `bootloader_components/slot_select/hooks.c` +6 −9 — esp_rom_printf("BOOT reason %d seq %u slot %d\n", reason, (unsigned)seq, slot);
- `main/web.c` +4 −5 — ESP_LOGW(TAG, "AP only SSID=%s ch=%u open", / ESP_LOGW(TAG, "softAP started SSID:%s password:(open) channel:%d", CUBE_WIFI_SSID, s_ap_channel);
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.4 / ## 2.8.4
- `BUILD_RELEASE.bat` +2 −2
- `main/config.h` +2 −2 — #define CUBE_FW_VERSION              "2.8.4" / #define CUBE_WIFI_CHANNEL            1
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.4", 4); / line(s_scr_sys, "СИСТЕМА 2.8.4", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.4
- `README_START_HERE_RU.txt` +1 −1
- и ещё 6 файлов

## FGMDCharge_FD_ROUND_V2_2.8.5.zip

Имя даёт номер 2.8.5. config.h: 2.8.5.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.4.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), загрузчик и слоты (1).
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.5 / ## 2.8.5
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.5 / ## 2.8.5
- `BUILD_RELEASE.bat` +2 −2
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.5", 4); / line(s_scr_sys, "СИСТЕМА 2.8.5", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.5
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.8.5
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.8.5
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.8.6.zip

Имя даёт номер 2.8.6. config.h: 2.8.6.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.5.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2), сайт (1).
- `main/web.c` +11 −6 — (void)snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", CUBE_WIFI_PASSWORD); / wifi_country_t country = { .cc = "CN", .schan = 1, .nchan = 13, .policy = WIFI_COUNTRY_POLICY_AUTO };
- `RELEASE_NOTES_RU.md` +8 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.6 / ## 2.8.6
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.6 / ## 2.8.6
- `BUILD_RELEASE.bat` +2 −2
- `main/config.h` +2 −2 — #define CUBE_FW_VERSION              "2.8.6" / #define CUBE_WIFI_CHANNEL            6
- `main/display.c` +2 −2 — line(s_scr_chg, "КУБ 2.8.6", 4); / line(s_scr_sys, "СИСТЕМА 2.8.6", 4);
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.6
- `README_START_HERE_RU.txt` +1 −1
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- и ещё 5 файлов

## FGMDCharge_FD_ROUND_V2_2.8.7.zip

Имя даёт номер 2.8.7. config.h: 2.8.7.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.6.zip`: документы (7), исходники (6), прошивка и скрипты (4), экран (3), сайт (1).
- `main/display.c` +161 −67 — if (s_pad_banner) lv_label_set_text(s_pad_banner, "пароль"); / if (strcmp(k, "<") == 0) {
- `main/web.c` +22 −9 — drop with reason 15 (Android shows that as "wrong password"). */ / wifi_country_t country = { .cc = "CN", .schan = 1, .nchan = 13, .policy = WIFI_COUNTRY_POLICY_MANUAL };
- `main/sm_relay.c` +15 −13
- `README_START_HERE_RU.txt` +20 −7
- `RELEASE_NOTES_RU.md` +12 −9 — # FGMDCharge / FD_ROUND V2 — Release 2.8.7 / ## 2.8.7
- `main/app_state.c` +13 −3 — if (s_state.protocol != 1) app_state_log("CHM -> GBT-2015"); / app_state_log("CRM without CHM -> GBT-2011");
- `GO.bat` +6 −4
- `tools/force_sdkconfig.py` +9 −0 — "CONFIG_ESP_WIFI_ENABLE_WPA3_SAE", / out.append(f"# {key} is not set")
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.7 / ## 2.8.7
- `BUILD_RELEASE.bat` +2 −2
- и ещё 11 файлов

## FGMDCharge_FD_ROUND_V2_2.8.8.zip

Имя даёт номер 2.8.8. config.h: 2.8.8.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.7.zip`: документы (7), экран (3), исходники (2), прошивка и скрипты (2).
- `RELEASE_NOTES_RU.md` +9 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.8 / ## 2.8.8
- `AUDIT_REPORT_RU.md` +3 −3 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.8 / ## 2.8.8
- `main/display.c` +3 −3 — line(s_scr_sys, "СИСТЕМА 2.8.8", 4);
- `BUILD_RELEASE.bat` +2 −2
- `README_START_HERE_RU.txt` +2 −2
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.8
- `RELEASE_CHECKLIST_RU.txt` +1 −1
- `docs/BUILD_AND_INSTALL_RU.md` +1 −1 — # Сборка и установка Release 2.8.8
- `docs/SAFE_FLASH_RU.md` +1 −1 — # Безопасная первая загрузка Release 2.8.8
- и ещё 4 файлов

## FGMDCharge_FD_ROUND_V2_2.8.9.zip

Имя даёт номер 2.8.9. config.h: 2.8.9.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.8.zip`: документы (8), исходники (6), экран (3), прошивка и скрипты (3), сайт (2), загрузчик и слоты (2).
- `main/web_assets.c` +447 −391
- `main/safe_ota.c` +390 −44 — #include "lzss.h" / #include "esp_flash.h"
- `main/sm_relay.c` +41 −13 — *   State4 entry  -> GPIO6 = HIGH (prepare, first click of the relay test) / *   State5 + CCS >= 50.0V stable 3s -> GPIO6 = LOW, GPIO7 = HIGH (load, second click)
- `main/web.c` +20 −3 — httpd_resp_set_status(req, "500 Internal Server Error");
- `README_START_HERE_RU.txt` +10 −4
- `main/display.c` +4 −9 — app_state_log(s_arm_act == 1 ? "пароль принят, ещё раз удержать выход" : "пароль принят, ещё раз удержать Вт"); / s_dots = line(s_scr_chg, "пароль 1234 до выкл", 248);
- `GO.bat` +7 −2 — if not exist "original\factory_lzss.bin" goto fail
- `RELEASE_NOTES_RU.md` +8 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.9 / ## 2.8.9
- `BUILD_RELEASE.bat` +2 −2
- `main/safe_ota.h` +2 −1
- и ещё 14 файлов

## FGMDCharge_FD_ROUND_V2_2.8.10.zip

Имя даёт номер 2.8.10. config.h: 2.8.10.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.9.zip`: документы (7), исходники (5), экран (3), прошивка и скрипты (3), сайт (1).
- `main/web_assets.c` +77 −77
- `main/display.c` +97 −23 — snprintf(b, sizeof(b), "яркость %d%%", v); / if (s_orient_lbl) lv_label_set_text(s_orient_lbl, s_rot ? "экран 180" : "экран обычный");
- `main/sm_relay.c` +28 −27 — app_state_log(nb ? "нагрузка ВКЛ, держится" : "нагрузка ВЫКЛ, держится");
- `README_START_HERE_RU.txt` +11 −13
- `main/app_state.c` +12 −8 — if (proto_log == 1) app_state_log("CHM -> GBT-2015"); / else if (proto_log == 2) app_state_log("CRM without CHM -> GBT-2011");
- `RELEASE_NOTES_RU.md` +8 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.10 / ## 2.8.10
- `BUILD_RELEASE.bat` +2 −2
- `AUDIT_REPORT_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.10
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `GO.bat` +1 −1
- и ещё 9 файлов

## FGMDCharge_FD_ROUND_V2_2.8.11.zip

Имя даёт номер 2.8.11. config.h: 2.8.11.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.10.zip`: документы (8), исходники (4), протокол (4), прошивка и скрипты (3), экран (2), сайт (1).
- `main/web_assets.c` +164 −160
- `main/sm_relay.c` +130 −41 — #include "gbt_b.h" / app_state_log("TX BMV BMT BSP");
- `host_tests/test_core.py` +102 −0 — # Class A / B identifiers and the 2011 short frames. / # 250.0 A demand is encoded as 2500 + 4000, not as a negative current.
- `main/can_gbt.c` +24 −5 — #include "gbt_b.h" / snprintf(m, sizeof(m), "CHM ver %u.%u.%u", d[0], d[1], d[2]);
- `main/app_state.c` +16 −6 — app_state_log(protocol == 1 ? "protocol GBT-2015/A" : / protocol == 2 ? "protocol GBT-2011" :
- `main/display.c` +9 −8 — const char *pn[] = {"авто", "2015", "2011", "2023"}; / line(s_scr_sys, "СИСТЕМА 2.8.11", 4);
- `docs/CAN_MAP.md` +15 −0 — ## GB/T 27930 on this 250 kbit/s port (2.8.11)
- `RELEASE_NOTES_RU.md` +11 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.11 / ## 2.8.11
- `README_START_HERE_RU.txt` +2 −2
- `tools/static_audit.py` +2 −2 — VERSION = "2.8.11" / "main.c", "app_state.c", "can_gbt.c", "gbt_tp.c", "gbt_b.c", "display.c", "touch_cst816s.c",
- и ещё 12 файлов

## FGMDCharge_FD_ROUND_V2_2.8.12.zip

Имя даёт номер 2.8.12. config.h: 2.8.12.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.11.zip`: исходники (8), документы (8), прошивка и скрипты (3), сайт (2), экран (2), протокол (1).
- `main/web_assets.c` +615 −507
- `host_tests/test_core.py` +78 −0 — cfg = (Path(__file__).resolve().parents[1] / "main" / "config.h").read_text(encoding="utf-8") / assert '#define CUBE_FW_VERSION              "2.8.12"' in cfg
- `main/app_state.c` +38 −8 — #include "session_log.h" / int w = snprintf(out + o, (size_t)room, "%s\n", s_log[n % 24]);
- `main/web.c` +45 −1 — #include "session_log.h" / appendf(buf, sizeof(buf), &used, "{\"ok\":true,\"count\":%d,\"logs\":[", n);
- `main/sm_relay.c` +21 −12 — #include "session_log.h"
- `README_START_HERE_RU.txt` +18 −6
- `RELEASE_NOTES_RU.md` +18 −2 — # FGMDCharge / FD_ROUND V2 — Release 2.8.12 / ## 2.8.12
- `main/display.c` +6 −4 — s_can = line(s_scr_chg, "CAN вкл тишина", 264); / line(s_scr_sys, "СИСТЕМА 2.8.12", 4);
- `main/config.h` +7 −2 — #define CUBE_FW_VERSION              "2.8.12" / #define CUBE_TX_DEFAULT_ENABLED      1
- `README_RU.md` +4 −3 — # FGMDCharge / FD_ROUND V2 — Release 2.8.12
- и ещё 14 файлов

## FGMDCharge_FD_ROUND_V2_2.8.13.zip

Имя даёт номер 2.8.13. config.h: 2.8.13.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.12.zip`: документы (7), исходники (5), прошивка и скрипты (3), сайт (2), экран (2), протокол (1).
- `main/web_assets.c` +497 −487
- `main/session_log.c` +80 −73 — #include <stdlib.h> / (void)httpd_resp_set_type(req, "text/plain; charset=utf-8");
- `main/web.c` +75 −9 — #include "esp_heap_caps.h" / #include "lwip/sockets.h"
- `README_START_HERE_RU.txt` +5 −8
- `RELEASE_NOTES_RU.md` +9 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.13 / ## 2.8.13
- `main/main.c` +4 −3 — #include "esp_heap_caps.h" / ESP_LOGW(TAG, "READY %s CAN_TX=%d WiFi=%s heap=%u", CUBE_FW_VERSION,
- `main/sm_relay.c` +1 −3
- `AUDIT_REPORT_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.13
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.13
- и ещё 10 файлов

## FGMDCharge_FD_ROUND_V2_2.8.13 (1).zip

Имя даёт номер 2.8.13. config.h: 2.8.13.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.13.zip`: экран (1), сайт (1), прошивка и скрипты (1), документы (1).
- `BUILD_RELEASE.bat` +2 −2
- `main/web.c` +2 −1
- `GO.bat` +1 −1
- `RELEASE_NOTES_RU.md` +2 −0

## FGMDCharge_FD_ROUND_V2_2.8.14.zip

Имя даёт номер 2.8.14. config.h: 2.8.14.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.13 (1).zip`: документы (7), исходники (4), прошивка и скрипты (4), экран (3), сайт (2), протокол (1).
- `main/web_assets.c` +328 −295
- `main/web.c` +126 −21 — #include "dhcpserver/dhcpserver.h" / #include "unistd.h"
- `main/session_log.c` +23 −8 — "FD_ROUND_V2 %s\nфайл сессии CAN\nпричина: %s\nпротокол: %s\nрежим: %s\nпередача CAN: %s\n" / (void)snprintf(disp, sizeof(disp), "attachment; filename=\"cube-%d.txt\"", seq);
- `RELEASE_NOTES_RU.md` +12 −8 — # FGMDCharge / FD_ROUND V2 — Release 2.8.14 / ## 2.8.14
- `README_START_HERE_RU.txt` +4 −4
- `BUILD_RELEASE.bat` +2 −2
- `AUDIT_REPORT_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.14
- `DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt` +1 −1
- `GO.bat` +1 −1
- `README_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — Release 2.8.14
- и ещё 11 файлов

## FGMDCharge_FD_ROUND_V2_2.8.15.zip

Имя даёт номер 2.8.15. config.h: 2.8.15.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.14.zip`: документы (7), исходники (4), прошивка и скрипты (4), экран (3), сайт (2), протокол (1).
- `main/web_assets.c` +1053 −733
- `main/web.c` +35 −30 — snprintf(cem, sizeof(cem), "%02X%02X%02X%02X", / "\"proto_n\":%u,\"proto_lock\":%s,\"crm_status\":%u,"
- `main/sm_relay.c` +30 −9 — b[7] = 0x58; /* 600 min. Zero means "finished" and a lot of piles stop. */
- `main/display.c` +13 −12 — if (s_pass_title) lv_label_set_text(s_pass_title, "код"); / if (s_pad_banner) lv_label_set_text(s_pad_banner, "код");
- `RELEASE_NOTES_RU.md` +10 −10 — # FGMDCharge / FD_ROUND V2 — Release 2.8.15 / ## 2.8.15
- `host_tests/test_core.py` +5 −4 — # 250.0 A charge demand is negative: raw = 4000 - 2500. / # A positive raw is outside -400 A..0 A and public piles abort.
- `main/app_state.c` +5 −3 — the moment CCS said "not yet". CST, CEM and CSD are the real endings. */
- `README_START_HERE_RU.txt` +3 −3
- `BUILD_RELEASE.bat` +2 −2
- `AUDIT_REPORT_RU.md` +1 −1 — # FGMDCharge / FD_ROUND V2 — RELEASE 2.8.15
- и ещё 11 файлов

## FGMDCharge_FD_ROUND_V2_2.8.15_release.zip

Имя даёт номер 2.8.15. config.h: 2.8.15.
Относительно `FGMDCharge_FD_ROUND_V2_2.8.15.zip`: документы (2), исходники (1).
- `README.md` — новый файл
- `original/README.md` — новый файл
- `sdkconfig` — новый файл

## Не прошивки куба

- `CUBIK_DISPLAY_LCD_RE_FROM_DUMP.zip` — текстовых файлов 5. Это разбор или заметка, не дерево ESP-IDF.
- `CUBIK_FIRMWARE_RE_FINAL_ANALYTICS.zip` — текстовых файлов 8. Это разбор или заметка, не дерево ESP-IDF.
- `FGMDCharge_ALL_NOTES_AND_2.7.4.zip` — текстовых файлов 1. Это разбор или заметка, не дерево ESP-IDF.
- `FGMDCharge_FULL_2.7.4_AND_SUMMARYgrok.zip` — текстовых файлов 91. Это разбор или заметка, не дерево ESP-IDF.
- `cubik_firmware_reverse_engineering.zip` — текстовых файлов 14. Это разбор или заметка, не дерево ESP-IDF.
- `cubik_firmware_reverse_engineering_step17.zip` — текстовых файлов 63. Это разбор или заметка, не дерево ESP-IDF.
- `cubik_firmware_reverse_engineering_step18.zip` — текстовых файлов 67. Это разбор или заметка, не дерево ESP-IDF.
- `cubik_firmware_reverse_engineering_step19.zip` — текстовых файлов 71. Это разбор или заметка, не дерево ESP-IDF.
- `cubik_firmware_reverse_engineering_step20.zip` — текстовых файлов 75. Это разбор или заметка, не дерево ESP-IDF.
- `cubik_firmware_reverse_engineering_step21.zip` — текстовых файлов 80. Это разбор или заметка, не дерево ESP-IDF.
- `archive/reference/esp32-evse-*.zip` — чужой проект J1772, GPL-3, [dzurikmiroslav/esp32-evse](https://github.com/dzurikmiroslav/esp32-evse).
