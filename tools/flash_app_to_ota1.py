#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OTA_OFFSET = 0x1A0000
OTA_SIZE = 0x190000

p = argparse.ArgumentParser(description="Flash only the application into OTA1; bootloader/OTA0 stay untouched")
p.add_argument("--port", required=True)
p.add_argument("--app", required=True)
a = p.parse_args()

app = Path(a.app)
if not app.is_file():
    raise SystemExit(f"ERROR: application image not found: {app}")
data = app.read_bytes()
if len(data) < 24 or len(data) > OTA_SIZE or data[0] != 0xE9:
    raise SystemExit("ERROR: application image failed local magic/size check")

check = subprocess.run([sys.executable, str(ROOT / "tools/check_app_image.py"), str(app)])
if check.returncode:
    raise SystemExit(check.returncode)

cmd = [sys.executable, "-m", "esptool", "--chip", "esp32c3", "--port", a.port,
       "write-flash", hex(OTA_OFFSET), str(app)]
print(f"Writing {len(data)} bytes to OTA1 at 0x{OTA_OFFSET:X} (slot 0x{OTA_SIZE:X}).")
raise SystemExit(subprocess.call(cmd, cwd=ROOT))
