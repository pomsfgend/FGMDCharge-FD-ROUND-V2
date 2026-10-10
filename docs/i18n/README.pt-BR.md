# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · [中文](README.zh-CN.md) · [Español](README.es.md) · Português · [Deutsch](README.de.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

Firmware público do cubo redondo com ESP32-C3. No CAN do carregador CC ele responde como BMS do veículo. Não começa a carga como se fosse o poste.

Versão **2.8.17**. Pinos, mapa da flash e o texto completo: [README.md](../../README.md). Mudanças: [CHANGELOG.md](../../CHANGELOG.md). Ordem do trabalho: [CHRONOLOGY.md](../../CHRONOLOGY.md).

A árvore foi feita por vibe coding, por muito tempo, a partir do dump de fábrica em `original/` e de testes em postos reais. Não há um único autor. A imagem de fábrica não foi escrita aqui e não é MIT. O fabricante da placa não aparece: só `FD_ROUND` e o título `EVCHARGER`.

## Downloads

| Arquivo | Uso |
|---|---|
| [bin 2.8.17](../../release/FGMDCharge_FD_ROUND_V2.bin) | App atual. Gravar em `ota_1`, endereço `0x1A0000` |
| [pacote 2.8.16](../../archive/2.8.16/) | A anterior. O log dizia softAP e o telefone não via Cube |
| [dump de fábrica](../../original/) | Leitura de 4 MB. Não é código deste repositório |

Não apague o chip inteiro se a imagem de fábrica em `ota_0` precisa ficar.

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Compilar no Windows com ESP-IDF 5.0.9: `BUILD_RELEASE.bat`. Só ESP32-C3.

## Uso

Wi-Fi **Cube**, senha `12345678`, site `http://192.168.4.1/`. Ao ligar, **НОВАЯ**. Protocolo automático: quadro de versão → GB/T 27930-2023 B (ChaoJi), CHM → 2015 / 2023-A, só CRM → 2011. CAN a 250 kbit/s.

## Ajudar

Issues e pull requests em português, English, 中文, Русский, Español, Deutsch, Français, 日本語 ou 한국어. [Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues). Não cole MAC, caminho de casa nem dados pessoais. A senha do AP Cube pode ser citada.
