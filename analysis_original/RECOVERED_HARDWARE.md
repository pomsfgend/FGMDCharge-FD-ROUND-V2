# Recovered hardware map from dump1.bin

## MCU / firmware

- ESP32-C3
- ESP-IDF v5.0.9
- Project `FD_ROUND`
- App version `1`
- 4 MiB flash
- app entry 0x40380434

## LCD

ILI9341 through ESP-IDF esp_lcd / SPI:

```text
MOSI GPIO5
SCLK GPIO19
CS   GPIO4
DC   GPIO18
MISO -1
20 MHz
240×280 logical resolution
```

## Touch

High-confidence evidence of an I2C touch controller at:

```text
I2C0
SDA GPIO1
SCL GPIO8
100 kHz
address 0x15
ChipID register 0xA7
```

The exact touch-controller model and coordinate orientation should still be runtime-tested before relying on touch for safety-critical controls.

## CAN

```text
TX GPIO10
RX GPIO9
250000 bit/s
TWAI normal mode
RX filter accepts all
TX queue 10
RX queue 10
```

## BLE

```text
name: FGMDCharge
service: 0xFFE0
FFE1: Read/Write/Notify/Indicate, max 500
FFE2: Write, max 500
FFE3: Write, max 500
CCCD: 0x2902
```
