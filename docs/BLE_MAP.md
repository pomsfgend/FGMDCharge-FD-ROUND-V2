# BLE map

## Original identity

- Name: `FGMDCharge`
- Service: `0xFFE0`
- Characteristic FFE1: Read + Write + Notify + Indicate, max 500 bytes
- Characteristic FFE2: Write, max 500 bytes
- Characteristic FFE3: Write, max 500 bytes
- CCCD: `0x2902`

The old firmware contains logs for notify/indicate enable/disable and an OTA state machine.

## V2

V2 preserves the same UUID layout so existing discovery code can still find the service. Incoming writes are logged and exposed in state, but arbitrary incoming bytes are **not** treated as firmware-update instructions.

V2 uses Web OTA as the primary update path because its packet format is under our control and the inactive OTA slot is selected by the ESP-IDF OTA API.
