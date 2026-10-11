# Changelog

Current source and the only prebuilt image are **2.8.17**.

Older numbers below are the journal of what changed. Their trees were not kept. A tag `notes/x.y.z` points at this same documentation commit so the text has a permanent URL. The zip GitHub attaches to that tag is **not** the old firmware. Do not flash it as that version.

Журнал. Единственные исходники и готовый bin — **2.8.17**. Более ранние деревья не сохранились. Метка `notes/x.y.z` ведёт на этот же коммит с текстом, а не на старую прошивку. Архив с такой метки прошивать нельзя.

## 2.8.18-beta

Beta, not tested on hardware. Branch `review-fixes`, tag `v2.8.18-beta`. `main` and `release/` stay on 2.8.17.
Бета, на железе не проверена. Ветка `review-fixes`, метка `v2.8.18-beta`. `main` и `release/` остаются на 2.8.17.

Fixed / исправлено:

- `sdkconfig.defaults` used `CONFIG_ESP_WIFI_*` names, which IDF 5.0.9 does not have (there they are `CONFIG_ESP32_WIFI_*`). WPA3-SAE stayed on and the buffer/AMPDU lines were ignored. Likely part of the Wi-Fi problem. / Имена опций Wi-Fi в `sdkconfig.defaults` не существуют в IDF 5.0.9, строки молча игнорировались, WPA3-SAE оставался включён.
- Wi-Fi: protocol b/g/n and HT20 set after `set_mode(AP)` and before start; TX power 15 dBm (`CUBE_WIFI_TX_POWER_QDBM`); no PSRAM TX cache; AP_START/AP_STOP and heap in the log. / Протокол и полоса до старта, мощность 15 дБм, в логе AP_START/AP_STOP и свободная память.
- esp_timer task stack 2048 -> 3584. LVGL draw buffers 40 -> 20 lines (+19 KB heap). / Стек esp_timer 3584, буферы LVGL вдвое меньше (+19 КБ кучи).
- Session logs moved from 0x330000 to 0x3F8000. They overwrote the compressed factory backup used by OTA. / Логи сеансов больше не затирают сжатую копию заводской прошивки.
- Stop under load: BST is repeated every 10 ms and the load relay (GPIO7) opens at <= 5 A or after 1 s, not in the same tick. CAN silence (unplug) still opens at once. / Стоп: BST каждые 10 мс, реле нагрузки размыкается при токе <= 5 А или через 1 с.
- Manual load latch is cleared when the session ends. GPIO7 state is tracked in a variable (`gpio_get_level` on an output-only pin always read 0). / Ручная фиксация реле сбрасывается в конце сеанса; состояние GPIO7 хранится в переменной.
- BCL voltage clamped to the CML min..max window. / Напряжение BCL ограничено окном CML.
- Web: status buffer 2048, overflow in `/api/can/recent`, auth on `/api/boot/original`, zeroed raw CAN buffer. / Веб: мелкие исправления буферов и проверка ключа.

Unchanged / без изменений: spoof behaviour (setpoints reported as measured), default request 240 V / 50 A, load relay closes after the station voltage is >= 50 V and stable for 3 s.

## 2.8.17

Fixed / исправлено:

- SoftAP was up in the log and invisible to phones. Country, protocol and bandwidth were rewritten after `esp_wifi_start`, which stops beacons on this ESP32-C3. Those calls now happen before start.
- PMF capable was on. Many phones drop that beacon. PMF is off.
- Wi-Fi comes up with power-on again, as in 2.8.14.

## 2.8.16

Fixed:

- Radio starts as AP only, so an old station profile in NVS no longer pulls the channel off Cube.
- Short tap on «СТАРАЯ» no longer boots the factory image. It takes a long press.
- After power loss the bootloader opens the new firmware again.

Bug left in this build, fixed in 2.8.17:

- The phone still did not list Cube. The log said `softAP started`.

## 2.8.15

Fixed:

- DHCP no longer drops the gateway, so the phone stops asking for an address in a loop.
- Captive-portal probes get a short answer instead of the whole page.
- Wi-Fi password and the screen code are not drawn on the display.
- Charge current is negative, as GB/T requires. A positive value made public stations abort.
- Requested current is clamped to the station CML.
- A CCS pause does not end the session. CST, CEM and CSD do.
- GB/T 2011 long frames use priority 6 and version V1.0.
- Web section «Станция»: which protocol is active, and frames that are not on the home page.

## 2.8.14

No separate note was written. 2.8.17 treats this build as the last one where Cube stayed visible. The bug that hid the network was introduced after it.

Отдельной записи нет. В 2.8.17 эта сборка указана как последняя, где сеть Cube была видна. Поломка появилась позже.

## 2.8.13

Fixed:

- The site died at `error in send : 11` after a large buffer. Wi-Fi buffers were reduced.

Still broken until 2.8.15:

- The phone left the network because the cube did not answer name lookups.

## 2.8.12

Fixed:

- CAN transmit is on at boot. It no longer waits for a separate switch.
- Default protocol is auto.

Also in this drop: session log in the erased tail after `ota_1` (`0x330000`, 8 slots). The web log page can download the last file or all of them.

## 2.8.11

Added the GB/T set used on this 250 kbit/s port:

- 2011, no CHM/BHM, BRM 41 bytes, version 1.0.0, CML/CCS 6 bytes accepted, CSD from 4 bytes, BMV/BMT/BSP every 1 s.
- 2015 and 2023 class A, version 1.1, CHM selects it, BRM 49, BCP 13, BSM allows charge, BMV/BMT/BSP every 10 s.
- 2023 class B / ChaoJi, version frame PF 0x38, addresses 0x5F and 0x25, version 2.0.0. Discharge, auth, reserve and self-check are not selected.

Bug left until 2.8.12: CAN transmit still waited to be switched on.

## 2.8.10

- One setpoint for voltage and current from the screen and the site.
- A long press on «выход» no longer clicks both relays. The load relay toggles and stays. The prepare relay is not touched by that hold.
- Main screen shows CAN rx/tx and switches the status tile from auto to 2015 or 2011.

## 2.8.9

Fixed:

- OTA stuck at 1%. The cube was already running from `ota_1` and refused to write that slot. The new image is written to `ota_0`, moved to `ota_1` after reboot, and the factory image is put back in `ota_0`.
- Screen code is remembered until power-off.
- Load relay is GPIO7. It closes when station voltage has been stable for about 3 seconds, and the prepare relay opens. Voltage loss opens the load.

## 2.8.8

The notes under this number also describe older build breaks. They are kept as written, not renumbered.

- LVGL 8.3 has no `lv_indev_set_long_press_time`. Hold is `long_press_time` on the driver.
- Phone associated and dropped 4 seconds later with reason 15 (wrong password). PMF was off while WPA3 was on, and `set_protocol` wiped the PSK. Switched to WPA2-PSK.
- Background Wi-Fi scan hopped channels and killed Cube. Scan runs only when the list is opened.
- `lv_font_montserrat_12` was not in sdkconfig. Replaced with montserrat_14.
- `apply_field` was used before its prototype. The build stopped on an implicit declaration.
- LVGL heap 32 KiB was exhausted by the keyboard. `lv_label_set_text` asserted and the idle task was watchedogged. Heap 64 KiB, malloc assert off.
- `snprintf` into 180 bytes for an OTA URL did not fit the worst case (`-Werror=format-truncation`). UNC paths are rewritten with `memcpy`.

## 2.7.4

Boot slot is written and the chip resets. One vertical screen. Return from the factory image needed three power cycles because the factory app rewrites the boot slot.

## 2.7.3

Font 14 so the 2.7.2 build could finish. A monitor log from that day still showed the factory `FD_ROUND` image, not this firmware.

## 2.7.2

Background scan removed, PMF off, backspace deletes one character. Build failed: missing montserrat 12. Fixed in 2.7.3.

## 2.7.1

Build succeeded. Cube was up. A scan every 20 seconds killed the AP and filled the log. The phone did not join.

## 2.7.0

Numeric fields instead of V+/A− buttons, network list up to 16, swipe only on the side strips. Build failed: `apply_field` called before its prototype. Fixed in 2.7.1.

## 2.6.0

LVGL pool raised to 64 KiB. At 32 KiB `lv_label_set_text` hung the main task.

## 2.5.9

Build failed on format-truncation in `web.c`. `snprintf` replaced with `memcpy`.

## 2.5.7

First full engineering tree: Bluedroid link errors, web assets compiled into `web_assets.c` instead of linker blobs, GATT table and CCCD, BLE advertising buffer made static, CAN 250 kbit/s with bus-off recovery, OTA only into the inactive slot, host tests.

Left open on purpose then: ADC/PWM pin map and the factory BLE OTA frame were not treated as proven from the dump, so unknown power commands were not sent by themselves.

## 2.3.0–2.5.8

Early builds: audit, local web, two OTA slots, GO.bat. Per-version trees were not saved. Details that exist are in `HISTORY_ALL_CHATS_RU.md`.

## Open

- ESP32-S3 is documented and not built.
- The factory image has no company name.
- Source trees older than 2.8.17 are not in git. The order of the work is [CHRONOLOGY.md](CHRONOLOGY.md). `analysis_original/` from the 2.1.x manifest is missing. A folder named `cube file` was not on the machine that published this.
