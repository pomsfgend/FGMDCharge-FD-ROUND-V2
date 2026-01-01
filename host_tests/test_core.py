#!/usr/bin/env python3
"""Host-side regression tests for algorithms that do not require ESP32 hardware."""

import struct
import zlib
from pathlib import Path
from dataclasses import dataclass, field

GBT_ID_TP_CM_CHG = 0x1CECF456
GBT_ID_TP_DT_CHG = 0x1CEBF456
GBT_ID_TP_CM_BAT = 0x1CEC56F4
GBT_ID_TP_DT_BAT = 0x1CEB56F4
GBT_TP_MAX_PAYLOAD = 0x6F0


def le16(p: bytes) -> int:
    assert len(p) >= 2
    return p[0] | (p[1] << 8)


def current_0p1a(raw: int) -> int:
    return raw - 4000


def cml(d: bytes):
    assert len(d) >= 8
    return (le16(d[0:2]), le16(d[2:4]), current_0p1a(le16(d[4:6])), current_0p1a(le16(d[6:8])))


def ccs(d: bytes):
    assert len(d) >= 7
    return le16(d[0:2]), current_0p1a(le16(d[2:4])), le16(d[4:6]), d[6] & 0x03


def bcl(d: bytes):
    assert len(d) >= 5
    return le16(d[0:2]), current_0p1a(le16(d[2:4])), d[4]


def ota_crc(seq: int) -> int:
    return zlib.crc32(struct.pack("<I", seq), 0xFFFFFFFF) & 0xFFFFFFFF


@dataclass
class TP:
    active: bool = False
    total: int = 0
    received: int = 0
    next_seq: int = 1
    data: bytearray = field(default_factory=lambda: bytearray(GBT_TP_MAX_PAYLOAD))

    def reset(self) -> None:
        self.active = False
        self.total = 0
        self.received = 0
        self.next_seq = 1
        self.data[:] = b"\x00" * len(self.data)

    def consume(self, can_id: int, frame: bytes):
        assert 0 <= len(frame) <= 8
        if can_id in (GBT_ID_TP_CM_CHG, GBT_ID_TP_CM_BAT):
            if len(frame) < 5 or frame[0] not in (0x10, 0x20):
                return None
            self.total = frame[1] | (frame[2] << 8)
            if not 0 < self.total <= GBT_TP_MAX_PAYLOAD:
                self.reset()
                return None
            self.active = True
            self.received = 0
            self.next_seq = 1
            return None
        if can_id not in (GBT_ID_TP_DT_CHG, GBT_ID_TP_DT_BAT) or not self.active or len(frame) < 2:
            return None
        if frame[0] != self.next_seq:
            self.reset()
            return None
        copy = min(len(frame) - 1, self.total - self.received)
        self.data[self.received:self.received + copy] = frame[1:1 + copy]
        self.received += copy
        self.next_seq = (self.next_seq + 1) & 0xFF
        if self.received >= self.total:
            payload = bytes(self.data[:self.total])
            self.active = False
            return payload
        return None


# Standard typed field sanity checks based on GB/T 27930 examples.
assert le16(bytes([0x58, 0x1B])) == 7000
assert cml(bytes([0x58, 0x1B, 0xD0, 0x07, 0xD8, 0x0E, 0xA0, 0x0F])) == (7000, 2000, -200, 0)
assert ccs(bytes([0x07, 0x00, 0xA0, 0x0F, 0xEB, 0x17, 0xFD])) == (7, 0, 6123, 1)
assert bcl(bytes([0x68, 0x10, 0xDE, 0x0D, 0x02])) == (4200, -450, 2)

# Known original OTA sequence 3 has this CRC in dump1.
assert ota_crc(3) == 0xED4A5011
assert ota_crc(1) != ota_crc(2)

# GB/T transport reassembly regression test.
tp = TP()
payload = bytes((i * 7) & 0xFF for i in range(33))
tp.consume(GBT_ID_TP_CM_CHG, bytes([0x20, len(payload), 0, 5, 0, 0, 0, 0]))
assembled = None
seq = 1
for pos in range(0, len(payload), 7):
    chunk = payload[pos:pos + 7]
    frame = bytes([seq]) + chunk
    assembled = tp.consume(GBT_ID_TP_DT_CHG, frame)
    seq += 1
assert assembled == payload

# Out-of-order packet must invalidate the active transfer.
tp = TP()
tp.consume(GBT_ID_TP_CM_BAT, bytes([0x20, 10, 0, 2, 0, 0, 0, 0]))
assert tp.consume(GBT_ID_TP_DT_BAT, bytes([2, 1, 2, 3])) is None
assert not tp.active

# Maximum payload and a full-size tail packet.
tp = TP()
payload = bytes((i * 13 + 3) & 0xFF for i in range(GBT_TP_MAX_PAYLOAD))
tp.consume(GBT_ID_TP_CM_CHG,
           bytes([0x20, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF, 254, 7, 0, 0, 0]))
assembled = None
seq = 1
for pos in range(0, len(payload), 7):
    assembled = tp.consume(GBT_ID_TP_DT_CHG, bytes([seq]) + payload[pos:pos + 7])
    seq += 1
assert assembled == payload


def b_id(p, pf, ps, sa):
    return ((p & 7) << 26) | (pf << 16) | (ps << 8) | sa


def cml_any(d: bytes):
    assert len(d) >= 6
    max_v, min_v = le16(d[0:2]), le16(d[2:4])
    max_i = current_0p1a(le16(d[4:6]))
    min_i = current_0p1a(le16(d[6:8])) if len(d) >= 8 else 0
    return max_v, min_v, max_i, min_i


def ccs_any(d: bytes):
    assert len(d) >= 6
    permit = d[6] & 0x03 if len(d) >= 7 else 0x01
    return le16(d[0:2]), current_0p1a(le16(d[2:4])), le16(d[4:6]), permit


def bcp_layout(cell, imax, energy, vmax, temp, soc_01, vnow):
    b = bytearray(13)
    b[0] = cell & 0xFF
    b[1] = (cell >> 8) & 0xFF
    b[2] = imax & 0xFF
    b[3] = (imax >> 8) & 0xFF
    b[4] = energy & 0xFF
    b[5] = (energy >> 8) & 0xFF
    b[6] = vmax & 0xFF
    b[7] = (vmax >> 8) & 0xFF
    b[8] = temp
    b[9] = soc_01 & 0xFF
    b[10] = (soc_01 >> 8) & 0xFF
    b[11] = vnow & 0xFF
    b[12] = (vnow >> 8) & 0xFF
    return bytes(b)


def bsm_status():
    bits = (1 << 12) | (3 << 14)
    return bytes([1, 75, 1, 70, 1, bits & 0xFF, (bits >> 8) & 0xFF])


def bem(which):
    d = bytearray([0xF0, 0xF0, 0xF0, 0xFC])
    if which == 0:
        d[0] = 0xF1
    elif which == 1:
        d[0] = 0xF4
    elif which == 2:
        d[1] = 0xF1
    elif which == 3:
        d[1] = 0xF4
    elif which == 4:
        d[2] = 0xF1
    elif which == 5:
        d[2] = 0xF4
    else:
        d[3] = 0xFD
    return bytes(d)


def detect(can_id, protocol):
    body = can_id & 0x00FFFFFF
    if body == 0x0038255F:
        return 3
    if (can_id & 0x00FFFF00) == 0x0026F400 and protocol != 3:
        return 1
    if (can_id & 0x00FFFF00) == 0x0001F400 and protocol == 0:
        return 2
    return protocol


# Class A / B identifiers and the 2011 short frames.
assert (0x1826F456 & 0x00FFFF00) == 0x0026F400
assert (0x1801F456 & 0x00FFFF00) == 0x0001F400
assert (b_id(3, 0x38, 0x25, 0x5F) & 0x00FFFFFF) == 0x0038255F
assert b_id(3, 0x38, 0x5F, 0x25) == 0x0C385F25
assert b_id(6, 0x36, 0x5F, 0x25) == 0x18365F25
assert b_id(4, 0x35, 0x5F, 0x25) == 0x10355F25
assert b_id(6, 0x34, 0x5F, 0x25) == 0x18345F25
assert b_id(3, 0x37, 0x5F, 0x25) == 0x0C375F25

assert cml_any(bytes([0x58, 0x1B, 0xD0, 0x07, 0xD8, 0x0E])) == (7000, 2000, -200, 0)
assert cml_any(bytes([0x58, 0x1B, 0xD0, 0x07, 0xD8, 0x0E, 0xA0, 0x0F])) == (7000, 2000, -200, 0)
assert ccs_any(bytes([0x07, 0x00, 0xA0, 0x0F, 0xEB, 0x17])) == (7, 0, 6123, 1)
assert ccs_any(bytes([0x07, 0x00, 0xA0, 0x0F, 0xEB, 0x17, 0xFD])) == (7, 0, 6123, 1)

# 250.0 A demand is encoded as 2500 + 4000, not as a negative current.
bcp = bcp_layout(420, 2500 + 4000, 75, 4500, 90, 200, 2400)
assert len(bcp) == 13
assert le16(bcp[2:4]) == 6500
assert le16(bcp[9:11]) == 200
assert bsm_status() == bytes([1, 75, 1, 70, 1, 0x00, 0xD0])
assert (bsm_status()[6] >> 4) & 0x03 == 0x01
assert bem(0)[0] & 0x03 == 0x01 and bem(2)[1] & 0x03 == 0x01 and bem(3)[1] & 0x0C == 0x04

assert detect(b_id(3, 0x38, 0x25, 0x5F), 1) == 3
assert detect(0x1826F456, 0) == 1
assert detect(0x1826F456, 3) == 3
assert detect(0x1801F456, 0) == 2
assert detect(0x1801F456, 1) == 1

def detect_user(can_id, protocol, locked):
    if locked:
        return protocol
    return detect(can_id, protocol)

def after_unplug(protocol, locked):
    return protocol if locked else 0

assert detect_user(0x1826F456, 2, True) == 2
assert detect_user(0x1826F456, 0, False) == 1
assert detect_user(b_id(3, 0x38, 0x25, 0x5F), 1, False) == 3
assert after_unplug(1, False) == 0
assert after_unplug(1, True) == 1

cfg = (Path(__file__).resolve().parents[1] / "main" / "config.h").read_text(encoding="utf-8")
assert '#define CUBE_FW_VERSION              "2.8.13"' in cfg
assert "#define CUBE_TX_DEFAULT_ENABLED      1" in cfg

def crc32_zip(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            mask = -(crc & 1) & 0xFFFFFFFF
            crc = ((crc >> 1) ^ (0xEDB88320 & mask)) & 0xFFFFFFFF
    return (~crc) & 0xFFFFFFFF

assert crc32_zip(b"cube") == zlib.crc32(b"cube") & 0xFFFFFFFF

def put_u16(buf, off, val):
    buf[off] = val & 0xFF
    buf[off + 1] = (val >> 8) & 0xFF

def put_u32(buf, off, val):
    put_u16(buf, off, val)
    put_u16(buf, off + 2, val >> 16)

def build_store_zip(files):
    blob = bytearray()
    central = bytearray()
    for name, data in files:
        name_b = name.encode()
        crc = crc32_zip(data)
        local = bytearray(30)
        put_u32(local, 0, 0x04034B50)
        put_u16(local, 4, 20)
        put_u32(local, 14, crc)
        put_u32(local, 18, len(data))
        put_u32(local, 22, len(data))
        put_u16(local, 26, len(name_b))
        off = len(blob)
        blob += local + name_b + data
        cd = bytearray(46)
        put_u32(cd, 0, 0x02014B50)
        put_u16(cd, 4, 20)
        put_u16(cd, 6, 20)
        put_u32(cd, 16, crc)
        put_u32(cd, 20, len(data))
        put_u32(cd, 24, len(data))
        put_u16(cd, 28, len(name_b))
        put_u32(cd, 42, off)
        central += cd + name_b
    end = bytearray(22)
    put_u32(end, 0, 0x06054B50)
    put_u16(end, 8, len(files))
    put_u16(end, 10, len(files))
    put_u32(end, 12, len(central))
    put_u32(end, 16, len(blob))
    raw = bytes(blob + central + end)
    zf = __import__("zipfile").ZipFile(__import__("io").BytesIO(raw))
    for name, data in files:
        assert zf.read(name) == data
    return raw

build_store_zip([("cube-1-2015-stop.txt", "протокол: GBT-2015/A\n".encode()),
                 ("cube-2-auto-proto.txt", "авто\n".encode())])

print("host core tests: PASS")
