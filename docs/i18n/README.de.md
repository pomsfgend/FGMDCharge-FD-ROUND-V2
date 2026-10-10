# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · [中文](README.zh-CN.md) · [Español](README.es.md) · [Português](README.pt-BR.md) · Deutsch · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

Öffentliche Firmware für den runden ESP32-C3-Würfel. Am CAN der DC-Säule antwortet er als Fahrzeug-BMS. Er startet keine Ladung als Säule.

Stand **2.8.17**. Pins, Flash-Karte und der lange Text: [README.md](../../README.md). Änderungen: [CHANGELOG.md](../../CHANGELOG.md). Ablauf: [CHRONOLOGY.md](../../CHRONOLOGY.md).

Der Baum ist über lange Zeit per Vibe-Coding aus dem Werksdump in `original/` und aus Tests an echten Säulen entstanden. Es gibt keinen einzelnen Autor. Das Werksimage wurde hier nicht geschrieben und steht nicht unter MIT. Der Platinenhersteller ist unbekannt: im Image stehen nur `FD_ROUND` und der Titel `EVCHARGER`.

## Downloads

| Datei | Wofür |
|---|---|
| [Bin 2.8.17](../../release/FGMDCharge_FD_ROUND_V2.bin) | Aktuelle App. Nach `ota_1`, Offset `0x1A0000` |
| [Paket 2.8.16](../../archive/2.8.16/) | Die vorherige. Log sagte softAP, das Telefon sah Cube nicht |
| [Werksdump](../../original/) | 4-MB-Auslesung. Kein Quelltext dieses Repos |

Den Chip nicht ganz löschen, wenn das Werksimage in `ota_0` bleiben soll.

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Bauen unter Windows mit ESP-IDF 5.0.9: `BUILD_RELEASE.bat`. Nur ESP32-C3.

## Betrieb

WLAN **Cube**, Passwort `12345678`, Seite `http://192.168.4.1/`. Beim Einschalten **НОВАЯ**. Protokoll automatisch: Versionsrahmen → GB/T 27930-2023 B (ChaoJi), CHM → 2015 / 2023-A, nur CRM → 2011. CAN 250 kbit/s.

## Mitmachen

Issues und Pull Requests auf Deutsch, English, 中文, Русский, Español, Português, Français, 日本語 oder 한국어. [Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues). Keine MAC, keinen Heimpfad, keine persönlichen Daten. Das AP-Passwort von Cube darf genannt werden.
