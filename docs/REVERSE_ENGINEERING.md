# Reverse engineering status

## Confirmed

| Layer | Result |
|---|---|
| CPU | ESP32-C3 / RISC-V |
| IDF | v5.0.9 |
| App | FD_ROUND, version 1 |
| Flash | 4 MiB |
| OTA | 2 × 0x190000 |
| LCD | ILI9341 via esp_lcd, SPI 20 MHz |
| LCD pins | MOSI=5, SCLK=19, CS=4, DC=18, MISO=-1 |
| LCD logical size | 240×280 |
| CAN | TWAI, normal mode, 250 kbit/s |
| CAN pins | TX=10, RX=9 |
| BLE name | FGMDCharge |
| BLE service | 0xFFE0 |
| BLE chars | 0xFFE1, 0xFFE2, 0xFFE3 |
| LVGL | present |
| BLE OTA | present as ESP-IDF OTA calls |

## Strong evidence / needs hardware confirmation

- Touch controller: CST816S/CST8xx-like, I2C0 SDA=1 SCL=8, addr 0x15.
- Original GATT table handle order follows the standard 8-row attribute table layout.
- Charger-side GB/T message IDs exactly visible in the receive parser.

## Not fully recoverable statically

- Complete original BLE command semantics.
- Complete original BLE OTA framing.
- Exact ADC channel ↔ GPIO mapping.
- Exact PWM output GPIO and application behavior.
- Exact semantics of every internal RAM field.
- Full manufacturer project source.

These are deliberately not fabricated in V2.
