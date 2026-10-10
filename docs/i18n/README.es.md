# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · [中文](README.zh-CN.md) · Español · [Português](README.pt-BR.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

Firmware público del cubo redondo con ESP32-C3. En el CAN del cargador de continua responde como BMS del vehículo. No inicia la carga como si fuera el poste.

Versión **2.8.17**. Pines, mapa de flash y el texto largo: [README.md](../../README.md). Cambios: [CHANGELOG.md](../../CHANGELOG.md). Orden del trabajo: [CHRONOLOGY.md](../../CHRONOLOGY.md).

El árbol se hizo por vibe coding, durante mucho tiempo, a partir del volcado de fábrica en `original/` y de pruebas en postes reales. No hay un solo autor. La imagen de fábrica no se escribió aquí y no es MIT. El fabricante de la placa no aparece: solo `FD_ROUND` y el título `EVCHARGER`.

## Descargas

| Archivo | Para qué |
|---|---|
| [bin 2.8.17](../../release/FGMDCharge_FD_ROUND_V2.bin) | La app actual. Escríbela en `ota_1`, offset `0x1A0000` |
| [paquete 2.8.16](../../archive/2.8.16/) | La anterior. El log decía softAP y el teléfono no veía Cube |
| [volcado de fábrica](../../original/) | Lectura de 4 MB. No es código de este repo |

No borres el chip entero si quieres conservar la imagen de fábrica en `ota_0`.

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Compilar en Windows con ESP-IDF 5.0.9: `BUILD_RELEASE.bat`. Solo ESP32-C3.

## Uso

Wi-Fi **Cube**, clave `12345678`, web `http://192.168.4.1/`. Al encender, **НОВАЯ**. Protocolo automático: trama de versión → GB/T 27930-2023 B (ChaoJi), CHM → 2015 / 2023-A, solo CRM → 2011. CAN a 250 kbit/s.

## Colaborar

Issues y pull requests en español, English, 中文, Русский, Português, Deutsch, Français, 日本語 o 한국어. [Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues). No pegues la MAC, una ruta de tu casa ni datos personales. La clave del AP Cube sí se puede mencionar.
