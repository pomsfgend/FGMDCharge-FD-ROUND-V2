#!/usr/bin/env python3
from __future__ import annotations

import ast
import hashlib
import re
import struct
import zlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "2.3.8"
DUMP_SHA256 = "1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233"
MAX_OTA = 0x190000

REQUIRED = [
    "CMakeLists.txt", "partitions.csv", "sdkconfig.defaults", "requirements-tools.txt",
    "main/CMakeLists.txt", "main/idf_component.yml", "main/main.c", "main/config.h", "main/web_assets.c",
    "main/app_state.c", "main/app_state.h", "main/can_gbt.c", "main/can_gbt.h",
    "main/gbt_tp.c", "main/gbt_tp.h", "main/display.c", "main/display.h",
    "main/touch_cst816s.c", "main/touch_cst816s.h", "main/ble.c", "main/ble.h",
    "main/web.c", "main/web.h", "main/safe_ota.c", "main/safe_ota.h",
    "main/settings.c", "main/settings.h", "main/adc_pwm.c", "main/sm_relay.c", "main/sm_relay.h", "main/board_pins.h",
    "main/web/index.html", "main/web/style.css", "main/web/app.js",
    "host_tests/test_core.py", "tools/static_audit.py", "tools/write_release_manifest.py",
    "tools/check_app_image.py", "tools/ota_slot.py", "tools/flash_app_to_ota1.py",
    "tools/restore_full_dump.py", "original/dump1.bin",
]


def err(msg: str) -> None:
    print(f"ERROR: {msg}")
    raise SystemExit(1)


def read(rel: str) -> str:
    p = ROOT / rel
    if not p.is_file():
        err(f"missing {rel}")
    try:
        return p.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        err(f"not UTF-8 text: {rel}: {exc}")


for rel in REQUIRED:
    p = ROOT / rel
    if not p.is_file():
        err(f"missing {rel}")

# Binary backup integrity.
dump = ROOT / "original/dump1.bin"
if dump.stat().st_size != 0x400000:
    err(f"dump size is {dump.stat().st_size}, expected 4194304")
sha = hashlib.sha256(dump.read_bytes()).hexdigest()
if sha != DUMP_SHA256:
    err(f"original dump SHA256 mismatch: {sha}")

# Original OTA selector integrity. Entry 0 is expected to be the currently valid selector.
otadata = ROOT / "original/otadata_0xD000_0xF000.bin"
if not otadata.is_file() or otadata.stat().st_size != 0x2000:
    err("original otadata slice missing or wrong size")
otadata_b = otadata.read_bytes()
seq, = struct.unpack_from("<I", otadata_b, 0)
stored_crc, = struct.unpack_from("<I", otadata_b, 28)
calc_crc = zlib.crc32(struct.pack("<I", seq), 0xFFFFFFFF) & 0xFFFFFFFF
if stored_crc != calc_crc:
    err(f"otadata entry 0 CRC mismatch: stored=0x{stored_crc:08X} calc=0x{calc_crc:08X}")

# Cross-file version consistency.
cfg = read("main/config.h")
component = read("main/idf_component.yml")
sdk = read("sdkconfig.defaults")
cmake = read("main/CMakeLists.txt")
for needle, label in [
    (f'#define CUBE_FW_VERSION              "{VERSION}"', "config version"),
    (f'version: "{VERSION}"', "component version"),
    (VERSION, "README/release version"),
]:
    if label == "README/release version":
        continue
    text = cfg if label == "config version" else component
    if needle not in text:
        err(f"{label} mismatch")

# Build-system invariants.
for src in re.findall(r'"([^"/]+\.c)"', cmake):
    if not (ROOT / "main" / src).is_file():
        err(f"CMake references missing source {src}")
for asset in ("web/index.html", "web/style.css", "web/app.js"):
    if not (ROOT / "main" / asset).is_file():
        err(f"web asset missing: {asset}")
if '"web_assets.c"' not in cmake or not (ROOT / "main/web_assets.c").is_file():
    err("compiled web asset source is not wired")
if "EMBED_FILES" in cmake or "EMBED_TXTFILES" in cmake:
    err("web assets must use compiled-in C arrays, not linker embedding")
if '"web_assets.c"' not in cmake:
    err("web_assets.c is not part of the component")
if "esp_lcd_ili9341" not in cmake or "bt" not in cmake:
    err("missing LCD/BLE component requirements")

# Target/config invariants.
for needle in (
    'CONFIG_IDF_TARGET="esp32c3"',
    "CONFIG_BT_ENABLED=y",
    "CONFIG_BT_CONTROLLER_ENABLED=y",
    "CONFIG_BT_BLUEDROID_ENABLED=y",
    "CONFIG_BT_BLE_ENABLED=y",
    "CONFIG_BT_GATTS_ENABLE=y",
    "CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y",
    "CONFIG_BT_BLE_42_ADV_EN=y",
    "CONFIG_BT_BLE_50_FEATURES_SUPPORTED=n",
    "CONFIG_BT_CLASSIC_ENABLED=n",
    "CONFIG_BT_NIMBLE_ENABLED=n",
):
    if needle not in sdk:
        err(f"sdkconfig.defaults missing {needle}")

# Safety/config invariants.
if '#define CUBE_TX_DEFAULT_ENABLED      0' not in cfg:
    err("CAN TX default is not disabled")
if "0x190000" not in cfg:
    err("OTA slot limit missing")

# Source-level stale/unsafe patterns that caused previous release failures.
for rel in sorted((ROOT / "main").rglob("*.c")):
    text = rel.read_text(encoding="utf-8")
    if "ESP_LE_ADV_FLAG_" in text:
        err(f"stale BLE flag in {rel.relative_to(ROOT)}")
    if "EMBED_TXTFILES" in text:
        err(f"stale EMBED_TXTFILES in {rel.relative_to(ROOT)}")
    if re.search(r"\b(strcpy|strcat|sprintf|vsprintf)\s*\(", text):
        err(f"unbounded string API in {rel.relative_to(ROOT)}")

can = read("main/can_gbt.c")
ble = read("main/ble.c")
for needle in (
    "CONFIG_BT_BLUEDROID_ENABLED", "ESP_BLE_ADV_FLAG_GEN_DISC",
    "ESP_BLE_ADV_FLAG_BREDR_NOT_SPT", "esp_ble_gap_config_adv_data",
    "esp_ble_gap_start_advertising", "esp_ble_gatts_create_attr_tab",
):
    if needle not in ble:
        err(f"BLE implementation missing {needle}")

web = read("main/web.c")
for needle in (
    "web_index_html", "web_style_css", "web_app_js",
    "HTTPD_SOCK_ERR_TIMEOUT",
):
    if needle not in web:
        err(f"web implementation missing {needle}")

ota = read("main/safe_ota.c")
for needle in ("esp_ota_end", "esp_ota_set_boot_partition"):
    if needle not in ota:
        err(f"OTA implementation missing {needle}")

display = read("main/display.c")
for needle in ("on_color_trans_done", "s_display", "lv_disp_flush_ready(s_display->driver)", "touch_lvgl_read"):
    if needle not in display:
        err(f"display/LVGL completion or touch hook missing {needle}")

if "TWAI_MSG_FLAG_EXTD" not in can or "id > 0x7FFu" not in can:
    err("CAN standard/extended ID selection missing")

# Partition map is exact for the reconstructed 4MiB OTA layout.
part = read("partitions.csv")
for needle in (
    "ota_0,      app,  ota_0,   0x10000,  0x190000",
    "ota_1,      app,  ota_1,   0x1A0000, 0x190000",
):
    if needle not in part:
        err(f"partition mismatch: {needle}")

# Web asset sanity and local-only policy.
html = read("main/web/index.html")
css = read("main/web/style.css")
js = read("main/web/app.js")
for rel, text in (("html", html), ("css", css), ("js", js)):
    if len(text) < 1000:
        err(f"web {rel} unexpectedly small")
    if "https://" in text:
        err(f"web {rel} contains external HTTPS dependency")
if "prefers-reduced-motion" not in css:
    err("web CSS lacks reduced-motion fallback")
if "XMLHttpRequest" not in js or "upload.onprogress" not in js:
    err("web OTA upload is not progress-capable")
if "0x190000" not in js:
    err("web OTA size guard missing")

# Python syntax/import-safe parse all release tools/tests.
for p in list((ROOT / "tools").glob("*.py")) + [ROOT / "host_tests/test_core.py"]:
    try:
        ast.parse(p.read_text(encoding="utf-8"), filename=str(p))
    except SyntaxError as exc:
        err(f"Python syntax error in {p.relative_to(ROOT)}: {exc}")

# If a built app exists, verify it is a plausible ESP image and fits OTA1.
built = ROOT / "release/FGMDCharge_FD_ROUND_V2.bin"
if built.exists():
    data = built.read_bytes()
    if not 24 <= len(data) <= MAX_OTA:
        err(f"built image size invalid: {len(data)}")
    if data[:1] != b"\xE9":
        err("built image does not start with ESP image magic 0xE9")
    if not 1 <= data[1] <= 16:
        err(f"built image segment count suspicious: {data[1]}")
    chip_id = struct.unpack_from("<H", data, 12)[0]
    if chip_id != 0x0005:
        err(f"built image chip_id mismatch: 0x{chip_id:04X}, expected ESP32-C3 0x0005")
    print(f"firmware size={len(data)} bytes sha256={hashlib.sha256(data).hexdigest()}")
else:
    print("firmware artifact not present yet (expected before first build)")

print("release verification: PASS")
