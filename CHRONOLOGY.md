# Chronology

Dates are the days the notes were written, not a lab log. Personal paths and the board MAC are left out.

Хронология по заметкам и переписке. Домашние пути и MAC платы сюда не входят.

## What is actually stored / Что реально лежит

| Where | What |
|---|---|
| `main` | Source and factory dump as of 2.8.17. Prebuilt `release/FGMDCharge_FD_ROUND_V2.bin`. |
| `notes/x.y.z` | Text only. Those tags are not old source. |
| Local copy from 9 Oct 2026, not in git | `FGMDCharge_2.8.16_src.zip` (version string 2.8.16) and its bin. `FGMDCharge_2.8.17_src.zip` matches the tree that became `main`. |
| Listed in `RELEASE_MANIFEST_SHA256.txt` and `FILE_INDEX.txt` (release 2.1.1 / 2.1.4) and **absent** now | Whole directory `analysis_original/`: disassembly of CAN, BLE, LCD, ADC/PWM, `FIRMWARE_REPORT_RU.md`, `KANONICAL_VALUES.md`, `RECOVERED_HARDWARE.md`. |

A folder named `cube file` is not on this machine and not in the GitHub repository. Nothing from an unseen folder was copied onto `main`.

## Line

1. Factory image read off the round cube. Project name in the image is `FD_ROUND`, on-screen title `EVCHARGER`. Manufacturer unknown. Chip ESP32-C3, 4 MB.
2. Around 2.0.6–2.1.4 the tree still carried `analysis_original/` (disassembly). That directory is not in 2.8.16 or 2.8.17.
3. 2.3.0–2.5.6. Local web, two OTA slots, `GO.bat`. Per-version trees were not kept. See `HISTORY_ALL_CHATS_RU.md`.
4. 2.5.7. First engineering tree that linked: BLE, web assets as C, CAN 250 kbit/s, OTA into the inactive slot only.
5. 2.5.8–2.5.9. Build died on `snprintf` format-truncation in `web.c`. Replaced with `memcpy`.
6. 2.6.0. LVGL pool 32 KiB hung `lv_label_set_text`. Raised to 64 KiB.
7. 2.7.0. Numeric fields, network list. Build failed: `apply_field` used before its prototype.
8. 2.7.1. Build ok. Cube was up. A scan every 20 s killed the AP. The phone did not join.
9. 2.7.2. Background scan removed, PMF off. Build failed: font montserrat 12 missing.
10. 2.7.3. Font 14. A monitor log that day still showed factory `FD_ROUND`, not this firmware.
11. 2.7.4. Boot slot write plus reset. One vertical screen. Summary dated 2026-10-08 still called 2.7.4 current.
12. 2.8.5. Power-on returns to the new/old menu. «СТАРАЯ» stays until power is removed.
13. 2.8.6. AP name Cube, channel 6. A zip of this number was named in a note and is not in the present files.
14. 2.8.8 notes (several, some describing 2.5–2.7). Reason 15 Wi-Fi drop: WPA3 plus a wiped PSK. Long-press API missing in LVGL 8.3. URL buffer too small.
15. 2.8.9. OTA no longer sticks at 1%. Screen code remembered until power-off. Load relay is GPIO7 after stable station voltage.
16. 2.8.10. One voltage/current setpoint. Hold on «выход» no longer clicks both relays. CAN counters on the main screen.
17. 2.8.11. GB/T 2011, 2015, 2023-A and 2023-B on one port. CAN transmit still had to be switched on.
18. 2.8.12. CAN transmit on at boot. Protocol auto. Session log in flash after `ota_1`, download one file or all of them.
19. 2.8.13. Site send error 11 reduced by smaller buffers. The phone still left Cube because names were not answered.
20. 2.8.14. No separate note. Later treated as the last build where Cube stayed visible.
21. 2.8.15. DHCP keeps the gateway. Captive probes get a short answer. Password and screen code removed from the display. Charge current encoded negative. CML clamp. CCS pause does not end the session. 2011 priority and version. Web section «Станция».
22. 2.8.16. AP only, long press for the factory image. Log said softAP started, the phone did not list Cube. Source zip of this exact tree is the local file above.
23. 2.8.17. Country and mode set before `esp_wifi_start`. PMF off. This is `main`.
24. After 2.8.17. Repository made public. Factory dump added, with the unknown-manufacturer note. Bilingual README, pin table, ESP32-S3 not built. Journal tags. Closed issues for the bugs above. RX shorted to TX on the module; that is a dead chip, not a firmware version.

## Not filled, because the files are not here

- `analysis_original/` from 2.1.x
- Source of 2.3.0 through 2.8.15, except what the journal already says
- The 2.8.6 zip named in the 2026-10-08 note
- Whatever sits in a folder called `cube file`

Those stay out of `main` until that folder is actually visible and you say to copy it.
