#!/usr/bin/env python3
import argparse
import hashlib
import pathlib
import struct

OTA_SIZE = 0x190000
ESP32C3_CHIP_ID = 0x0005

p = argparse.ArgumentParser(description='Static sanity check for an ESP32-C3 app image')
p.add_argument('image')
a = p.parse_args()
data = pathlib.Path(a.image).read_bytes()
if len(data) < 24:
    raise SystemExit('ERROR: image too small')
magic, seg_count, spi_mode, spi_speed_size, entry = struct.unpack_from('<BBBBI', data, 0)
chip_id = struct.unpack_from('<H', data, 12)[0]
print(f'file={a.image}')
print(f'size=0x{len(data):X} ({len(data)} bytes) sha256={hashlib.sha256(data).hexdigest()}')
print(f'magic=0x{magic:02X} segments={seg_count} chip_id=0x{chip_id:04X} entry=0x{entry:08X}')
if magic != 0xE9:
    raise SystemExit('ERROR: not an ESP image (magic != 0xE9)')
if not (1 <= seg_count <= 16):
    raise SystemExit('ERROR: suspicious segment count')
if chip_id != ESP32C3_CHIP_ID:
    raise SystemExit(f'ERROR: image target chip_id=0x{chip_id:04X}; expected ESP32-C3 chip_id=0x{ESP32C3_CHIP_ID:04X}')
off = 24
max_end = 0
for i in range(seg_count):
    if off + 8 > len(data):
        raise SystemExit('ERROR: truncated segment header')
    load, size = struct.unpack_from('<II', data, off)
    off += 8
    if off + size > len(data):
        raise SystemExit(f'ERROR: truncated segment {i}')
    max_end = max(max_end, off + size)
    print(f'seg{i}: load=0x{load:08X} size=0x{size:X} file_end=0x{off + size:X}')
    off += size
if len(data) > OTA_SIZE:
    raise SystemExit(f'ERROR: image exceeds OTA slot size 0x{OTA_SIZE:X}')
if max_end > OTA_SIZE:
    raise SystemExit('ERROR: image layout exceeds OTA slot')
needle = b'esp32c3'
print('esp32c3_marker=', 'FOUND' if needle in data else 'not found (not fatal)')
print('CHECK: PASS')
