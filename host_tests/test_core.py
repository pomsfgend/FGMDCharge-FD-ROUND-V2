#!/usr/bin/env python3
"""Host-side regression tests for algorithms that do not require ESP32 hardware."""

import struct
import zlib
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

print("host core tests: PASS")
