# GB/T CAN map recovered from the original cube firmware

## Charger → cube IDs visible in the old receive parser

```text
0x1807F456
0x1801F456
0x1826F456
0x100AF456
0x101AF456
0x181DF456
0x081FF456
0x1808F456
0x1812F456
```

The old parser first checks the source/destination bytes around `0x56/0xF4`, then dispatches the 29-bit identifier.

## Known state effects from the disassembly

The receive routine around `0x4200C4A8` writes to an internal state block around `0x3FC98200`.

Observed mappings include:

- `0x1826F456` → state offset `+0x18` timestamp/counter
- `0x1801F456` → state offset `+0x1C`; payload byte 6/derived field also stored at `+0x3C`
- `0x1807F456` → state offset `+0x20`
- `0x100AF456` → state offset `+0x28`; one payload field at `+0x3E`
- `0x1808F456` → state offset `+0x24`; raw voltage/current/time fields are transformed into state offsets `+0x0E`, `+0x10`, `+0x08`, `+0x0A`
- `0x1812F456` → state offset `+0x2C`; raw output voltage/current/time/status-related fields are stored at offsets `+0x12`, `+0x10`, `+0x08`, `+0x0A`
- `0x101AF456` → state offset `+0x30`
- `0x181DF456` → state offset `+0x34`
- `0x081FF456` → state offset `+0x38`

The exact byte-to-field meaning of every auxiliary internal field is not assumed. V2 therefore retains raw CAN frames as the authoritative record.

## Multi-frame evidence

A helper around `0x4200B5F8` maintains reassembly state around `0x3FC981E8`.
Observed control values include `0x11` and `0x13`, and maximum total length is `0x6F0` bytes.
V2 includes a generic reassembler. BMS answers on the new firmware are transmitted as soon as the station is recognized.

## GB/T 27930 on this 250 kbit/s port (2.8.13)

The cube is the vehicle (EVCC/BMS). On the new firmware TX is on at boot, protocol starts in AUTO and latches from the first identifying frame. A manual 2015 / 2011 / 2023 choice stays locked until AUTO is selected again. After the session ends an unlocked choice returns to AUTO. ISO 15118, CHAdeMO and cloud OCPP/云快充 do not share this port. The last session text is stored in the erased flash tail at 0x330000 (8 slots) and downloaded from the local site.

| Protocol | How it is recognized | What the cube sends |
|---|---|---|
| 2015 and 2023 class A | CHM `PF=0x26 PS=0xF4`, version bytes `01 01 00` | BHM, BRM 49, BCP 13, BRO, BCL, BCS, BSM, BMV/BMT/BSP every 10 s, BST, BSD, BEM |
| 2011 | CRM `PF=0x01 PS=0xF4` and no CHM, no class-B version frame | no BHM, BRM 41 version `01 00 00`, same charge loop, optional cells every 1 s. CML may be 6 bytes, CCS 6 bytes, CSD 5 bytes |
| 2023 class B | version frame `(id & 0x00FFFFFF) == 0x0038255F` | version answer `P=3 PF=0x38 SA=0x25 PS=0x5F`, then RM/URM |

Class A addresses: charger `0x56`, vehicle `0xF4`. Current is 0.1 A with offset −400 A. BCP SOC is two bytes in 0.1 %. BSM byte 6 is `0x00`, byte 7 is `0xD0` (charge allowed, reserved bits 1, temperatures about 20–25 °C).

Class B addresses: charger `0x5F`, vehicle `0x25`. Priority 3 version and control, priority 6 long and unreliable short, priority 4 reliable short. PF `0x38` version, `0x34` long, `0x35` reliable short, `0x36` unreliable short, `0x37` ack. Success only for CAN 2.0 and version 2.0.0. The vehicle picks charge FDC 1 and statistics FDC 1. It does not pick auth, reserve, self-check, supply or discharge. Current and voltage in B messages have offset 0. There is no measured-output CCS, so 3 s after the charger says ready or its contactor is closed the cube copies the setpoint into the CCS fields that already drive the load relay.

