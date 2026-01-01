# FGMDCharge / FD_ROUND V2

Open firmware for the round ESP32-C3 cube that speaks on the DC charging CAN port as the vehicle BMS.

Release **2.8.15**. Target: ESP32-C3, ESP-IDF **5.0.9**, flash 4 MB, CAN 250 kbit/s.

The cube does not start a charge by itself as a charger. It answers the station. On the new firmware, CAN transmit is already on. Protocol **auto** follows the first frame the station sends:

| What the station sends | Protocol |
|---|---|
| Version frame, addresses 0x5F / 0x25 | GB/T 27930-2023 class B (ChaoJi) |
| CHM | GB/T 27930-2015 and 2023 class A |
| CRM and neither of the above | GB/T 27930-2011 |

Charge current is encoded negative, as the standard requires. A CCS pause does not end the session. CST, CEM and CSD do.

## Use

1. Power on and choose **НОВАЯ**. **СТАРАЯ** is the factory image and has no website.
2. Wi-Fi **Cube**, password `12345678`, site `http://192.168.4.1/`.
3. Section **Станция** shows which protocol is active and the frames that are not on the home page.
4. Section **Логи** lists CAN session files. One file, or all of them as one archive.

Screen code `1234` is checked in firmware and is not printed on the display.

## Build

Windows, ESP-IDF 5.0.9 already installed:

```
BUILD_RELEASE.bat
```

The file to flash or to send by OTA is `release\FGMDCharge_FD_ROUND_V2.bin`. Do not flash the zip. Do not remove power until the cube reboots itself.

`GO.bat` builds and flashes by cable (bootloader, partition table, factory image in ota_0, this firmware in ota_1).

## What is not MIT

`original/` holds a backup of the factory chip image so the menu can still boot **СТАРАЯ**. Those binaries are not source code of this project and are not licensed under MIT. See [LICENSE](LICENSE).

## Layout

- `main/` — CAN, GB/T 2011 / 2015 / 2023-A / 2023-B, display, Wi-Fi, local site, OTA, session log
- `docs/` — build and flash notes in Russian
- `host_tests/` — protocol layout checks that run without the chip
- `RELEASE_NOTES_RU.md` — what changed in each release
