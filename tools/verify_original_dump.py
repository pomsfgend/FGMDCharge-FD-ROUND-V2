#!/usr/bin/env python3
from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
P = ROOT / "original/dump1.bin"
EXPECTED = "1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233"
if not P.is_file():
    print("ERROR: original/dump1.bin is missing")
    raise SystemExit(1)
if P.stat().st_size != 0x400000:
    print(f"ERROR: dump size {P.stat().st_size}; expected 4194304")
    raise SystemExit(1)
sha = hashlib.sha256(P.read_bytes()).hexdigest()
print(f"original dump SHA-256: {sha}")
if sha != EXPECTED:
    print("ERROR: dump hash mismatch; refusing full restore")
    raise SystemExit(1)
print("original dump verification: PASS")
