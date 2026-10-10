#!/usr/bin/env python3
import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233"

p = argparse.ArgumentParser(description="Restore complete ESP32-C3 flash from the verified original dump")
p.add_argument("--port", required=True)
p.add_argument("--dump", default="original/dump1.bin")
a = p.parse_args()

dump = (ROOT / a.dump).resolve()
if not dump.is_file():
    raise SystemExit(f"ERROR: dump not found: {dump}")
if dump.stat().st_size != 0x400000:
    raise SystemExit(f"ERROR: dump must be exactly 4 MiB, got {dump.stat().st_size}")
sha = hashlib.sha256(dump.read_bytes()).hexdigest()
if sha != EXPECTED:
    raise SystemExit("ERROR: dump SHA-256 mismatch; refusing full restore")

print("FULL RESTORE: this overwrites the entire 4MiB flash.")
cmd = [sys.executable, "-m", "esptool", "--chip", "esp32c3", "--port", a.port,
       "write_flash", "0x0", str(dump)]
raise SystemExit(subprocess.call(cmd, cwd=ROOT))
