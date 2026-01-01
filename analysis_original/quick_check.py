from pathlib import Path
import hashlib
import re

p = Path(__file__).resolve().parents[2] / "dump1.bin"
if not p.exists():
    print("Place dump1.bin next to the package root or change p in this script.")
    raise SystemExit(1)
b = p.read_bytes()
print("size:", len(b))
print("sha256:", hashlib.sha256(b).hexdigest())
for s in [b"FD_ROUND", b"v5.0.9", b"./main/CAN.c", b"./main/BLE.c", b"./main/ADC.c", b"./main/esp_lcd_ili9341.c"]:
    print(s.decode(), "FOUND" if s in b else "missing")
