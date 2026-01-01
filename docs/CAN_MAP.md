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
V2 includes a generic reassembler, but CAN TX of transport frames is not enabled by default.
