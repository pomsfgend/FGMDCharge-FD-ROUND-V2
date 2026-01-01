#!/usr/bin/env python3
import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description="Read a complete 4MiB ESP32-C3 flash backup")
p.add_argument("--port", required=True)
p.add_argument("--out", default="cube_backup_4mb.bin")
a = p.parse_args()

out = Path(a.out)
if not out.is_absolute():
    out = ROOT / out
out.parent.mkdir(parents=True, exist_ok=True)
cmd = [sys.executable, "-m", "esptool", "--chip", "esp32c3", "--port", a.port,
       "read-flash", "0x0", "0x400000", str(out)]
rc = subprocess.call(cmd, cwd=ROOT)
if rc != 0:
    raise SystemExit(rc)
if out.stat().st_size != 0x400000:
    raise SystemExit(f"ERROR: backup size is {out.stat().st_size}, expected 4194304")
print(f"backup={out}")
print(f"sha256={hashlib.sha256(out.read_bytes()).hexdigest()}")
