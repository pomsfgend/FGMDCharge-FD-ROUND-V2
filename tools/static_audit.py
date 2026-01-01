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
VERSION = "2.1.0"
DUMP_SHA = "1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233"
ERRORS: list[str] = []


def fail(msg: str) -> None:
    ERRORS.append(msg)


def read(rel: str) -> str:
    p = ROOT / rel
    if not p.is_file():
        fail(f"missing file: {rel}")
        return ""
    return p.read_text(encoding="utf-8", errors="strict")


def strip_c_comments_strings(text: str) -> str:
    out = []
    i = 0
    n = len(text)
    state = "code"
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == '/' and nxt == '/':
                state = "line_comment"; out.extend('  '); i += 2; continue
            if c == '/' and nxt == '*':
                state = "block_comment"; out.extend('  '); i += 2; continue
            if c == '"':
                state = "string"; out.append(' '); i += 1; continue
            if c == "'":
                state = "char"; out.append(' '); i += 1; continue
            out.append(c); i += 1; continue
        if state == "line_comment":
            if c == '\n': state = "code"; out.append('\n')
            else: out.append(' ')
            i += 1; continue
        if state == "block_comment":
            if c == '*' and nxt == '/': state = "code"; out.extend('  '); i += 2; continue
            out.append('\n' if c == '\n' else ' '); i += 1; continue
        if state == "string":
            if c == '\\':
                out.extend('  '); i += 2; continue
            if c == '"': state = "code"
            out.append(' '); i += 1; continue
        if state == "char":
            if c == '\\':
                out.extend('  '); i += 2; continue
            if c == "'": state = "code"
            out.append(' '); i += 1; continue
    return ''.join(out)


def balance_check(rel: str) -> None:
    text = strip_c_comments_strings(read(rel))
    pairs = {')': '(', ']': '[', '}': '{'}
    stack: list[str] = []
    for pos, c in enumerate(text):
        if c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack[-1] != pairs[c]:
                fail(f"delimiter mismatch in {rel} near character {pos}")
                return
            stack.pop()
    if stack:
        fail(f"unclosed delimiter(s) in {rel}: {''.join(stack)}")


def check_version() -> None:
    for rel in (
        "main/config.h", "main/idf_component.yml", "README_START_HERE_RU.txt", "README_RU.md",
        "RELEASE_NOTES_RU.md", "RELEASE_CHECKLIST_RU.txt", "docs/BUILD_AND_INSTALL_RU.md",
        "docs/SAFE_FLASH_RU.md", "DO_NOT_FLASH_WHOLE_CHIP_FIRST.txt", "AUDIT_REPORT_RU.md",
    ):
        if VERSION not in read(rel): fail(f"version {VERSION} missing from {rel}")


def check_cmake() -> None:
    cmake = read("main/CMakeLists.txt")
    expected_src = (
        "main.c", "app_state.c", "can_gbt.c", "gbt_tp.c", "display.c", "touch_cst816s.c",
        "ble.c", "web.c", "web_assets.c", "safe_ota.c", "settings.c", "adc_pwm.c",
        "sm_relay.c",
    )
    for src in expected_src:
        if f'"{src}"' not in cmake: fail(f"main/CMakeLists.txt missing source {src}")
        if not (ROOT / "main" / src).is_file(): fail(f"missing source file {src}")
    if "EMBED_TXTFILES" in cmake: fail("stale EMBED_TXTFILES in main/CMakeLists.txt")
    if "EMBED_FILES" in cmake: fail("unexpected linker embedding; web_assets.c is used")
    if '"web_assets.c"' not in cmake: fail("CMake missing web_assets.c")
    for asset in ("web/index.html", "web/style.css", "web/app.js"):
        if not (ROOT / "main" / asset).is_file(): fail(f"missing web asset {asset}")
    for req in ("esp_lcd_ili9341", "esp_http_server", "bt", "app_update", "lvgl"):
        if req not in cmake: fail(f"CMake missing component requirement: {req}")


def check_config() -> None:
    sdk = read("sdkconfig.defaults")
    for needle in (
        'CONFIG_IDF_TARGET="esp32c3"', 'CONFIG_ESPTOOLPY_FLASHSIZE="4MB"',
        'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"',
        "CONFIG_BT_ENABLED=y", "CONFIG_BT_CONTROLLER_ENABLED=y", "CONFIG_BT_BLUEDROID_ENABLED=y",
        "CONFIG_BT_BLE_ENABLED=y", "CONFIG_BT_GATTS_ENABLE=y",
        "CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y", "CONFIG_BT_BLE_42_ADV_EN=y",
        "CONFIG_BT_BLE_42_SCAN_EN=y", "CONFIG_BT_BLE_50_FEATURES_SUPPORTED=n",
        "CONFIG_BT_CLASSIC_ENABLED=n", "CONFIG_BT_NIMBLE_ENABLED=n",
    ):
        if needle not in sdk: fail(f"sdkconfig.defaults missing {needle}")
    cfg = read("main/config.h")
    for needle in (
        f'#define CUBE_FW_VERSION              "{VERSION}"',
        '#define CUBE_TX_DEFAULT_ENABLED      0',
        '#define CUBE_CAN_BITRATE             250000',
        '#define CUBE_HTTP_MAX_UPLOAD       (0x190000)',
    ):
        if needle not in cfg: fail(f"config invariant missing: {needle}")


def check_source_hygiene() -> None:
    for p in sorted((ROOT / "main").glob("*.c")):
        rel = p.relative_to(ROOT).as_posix()
        text = p.read_text(encoding="utf-8")
        balance_check(rel)
        for bad, label in (
            ("ESP_LE_ADV_FLAG_", "obsolete BLE flag"),
            ("EMBED_TXTFILES", "obsolete embed directive"),
        ):
            if bad in text: fail(f"{label} in {rel}")
        if re.search(r"\b(strcpy|strcat|sprintf|vsprintf)\s*\(", text):
            fail(f"unbounded string API in {rel}")
    for p in sorted((ROOT / "main").glob("*.h")):
        rel = p.relative_to(ROOT).as_posix()
        balance_check(rel)
        if "#pragma once" not in p.read_text(encoding="utf-8"):
            fail(f"header lacks #pragma once: {rel}")


def check_behavioral_hooks() -> None:
    ble = read("main/ble.c")
    for needle in (
        "ESP_BLE_ADV_FLAG_GEN_DISC", "ESP_BLE_ADV_FLAG_BREDR_NOT_SPT", "esp_ble_gap_config_adv_data",
        "esp_ble_gap_start_advertising", "esp_ble_gatts_create_attr_tab", "s_adv_data", "s_char1_prop",
    ):
        if needle not in ble: fail(f"BLE hook missing: {needle}")
    display = read("main/display.c")
    for needle in (
        "on_color_trans_done", "lv_disp_flush_ready(s_display->driver)", "touch_lvgl_read",
    ):
        if needle not in display: fail(f"LCD/LVGL hook missing: {needle}")
    can = read("main/can_gbt.c")
    for needle in ("TWAI_TIMING_CONFIG_250KBITS", "TWAI_STATE_BUS_OFF", "twai_initiate_recovery", "TWAI_STATE_STOPPED", "twai_start", "TWAI_MSG_FLAG_EXTD", "id > 0x7FFu"):
        if needle not in can: fail(f"CAN recovery hook missing: {needle}")
    ota = read("main/safe_ota.c")
    for needle in ("0xE9", "esp_ota_begin", "esp_ota_write", "esp_ota_end", "esp_ota_set_boot_partition"):
        if needle not in ota: fail(f"OTA hook missing: {needle}")
    web = read("main/web.c")
    for needle in (
        "web_index_html", "web_style_css", "web_app_js",
        "HTTPD_SOCK_ERR_TIMEOUT", "X-Content-Type-Options", "Content-Security-Policy",
    ):
        if needle not in web: fail(f"Web hook missing: {needle}")


def check_partitions_and_dump() -> None:
    part = read("partitions.csv")
    for needle in (
        "ota_0,      app,  ota_0,   0x10000,  0x190000",
        "ota_1,      app,  ota_1,   0x1A0000, 0x190000",
    ):
        if needle not in part: fail(f"partition mismatch: {needle}")
    dump = ROOT / "original/dump1.bin"
    if not dump.is_file() or dump.stat().st_size != 0x400000:
        fail("original/dump1.bin missing or not 4 MiB"); return
    data = dump.read_bytes()
    if hashlib.sha256(data).hexdigest() != DUMP_SHA:
        fail("original/dump1.bin SHA-256 mismatch")
    otadata = ROOT / "original/otadata_0xD000_0xF000.bin"
    if otadata.is_file() and otadata.stat().st_size == 0x2000:
        e0 = otadata.read_bytes()[:32]
        seq, = struct.unpack_from("<I", e0, 0)
        stored, = struct.unpack_from("<I", e0, 28)
        calc = zlib.crc32(struct.pack("<I", seq), 0xFFFFFFFF) & 0xFFFFFFFF
        if stored != calc:
            fail(f"otadata entry 0 CRC mismatch: stored=0x{stored:08X} calc=0x{calc:08X}")
    else:
        fail("otadata slice missing or wrong size")
    for rel, start, end in (
        ("original/bootloader_0x1000_0x8000.bin", 0x1000, 0x8000),
        ("original/partition_table_0x8000_0x9000.bin", 0x8000, 0x9000),
        ("original/nvs_0x9000_0xD000.bin", 0x9000, 0xD000),
        ("original/otadata_0xD000_0xF000.bin", 0xD000, 0xF000),
        ("original/phy_init_0xF000_0x10000.bin", 0xF000, 0x10000),
        ("original/ota0_partition_0x190000.bin", 0x10000, 0x1A0000),
        ("original/ota1_partition_0x190000.bin", 0x1A0000, 0x330000),
    ):
        p = ROOT / rel
        if not p.is_file(): fail(f"missing dump slice: {rel}"); continue
        expected = data[start:end]
        actual = p.read_bytes()
        if actual != expected: fail(f"dump slice does not match dump1.bin: {rel}")


def check_original_app_images() -> None:
    for rel in ("original/ota0_partition_0x190000.bin", "original/ota1_partition_0x190000.bin"):
        p = ROOT / rel
        if not p.is_file():
            fail(f"missing original app image: {rel}")
            continue
        b = p.read_bytes()
        if len(b) != 0x190000 or b[0] != 0xE9:
            fail(f"original app image invalid header/size: {rel}")
            continue
        chip_id, = struct.unpack_from("<H", b, 12)
        if chip_id != 0x0005:
            fail(f"original app image chip_id mismatch: {rel}: 0x{chip_id:04X}")


def check_web() -> None:
    html = read("main/web/index.html")
    css = read("main/web/style.css")
    js = read("main/web/app.js")
    assets = read("main/web_assets.c")
    # Decode the deterministic hexadecimal arrays and verify that the binary payload
    # embedded into firmware is exactly the current source tree content.
    for name, source in (("web_index_html", html.encode("utf-8")),
                         ("web_style_css", css.encode("utf-8")),
                         ("web_app_js", js.encode("utf-8"))):
        m = re.search(rf"const uint8_t {name}\[\] = \{{(.*?)\}};", assets, flags=re.S)
        if not m:
            fail(f"missing generated asset array: {name}")
            continue
        raw = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
        if raw != source + b"\0":
            fail(f"generated web asset is stale: {name}")
    for label, text in (("HTML", html), ("CSS", css), ("JS", js)):
        if len(text) < 1000: fail(f"web {label} unexpectedly small")
        if "https://" in text: fail(f"web {label} contains external HTTPS resource")
    html_wo_script = re.sub(r"<script[\s\S]*?</script>", "", html, flags=re.I)
    html_wo_script = re.sub(r"<style[\s\S]*?</style>", "", html_wo_script, flags=re.I)
    if re.search(r"\bon(?:click|load|error|input|change)\s*=", html_wo_script, flags=re.I): fail("inline HTML event handler found")
    for needle in ("prefers-reduced-motion", ".metric-card:hover", "@keyframes"):
        if needle not in css: fail(f"web CSS hook missing: {needle}")
    for needle in ("XMLHttpRequest", "upload.onprogress", "xhr.timeout", "0x190000"):
        if needle not in js: fail(f"web JS OTA hook missing: {needle}")


def check_python() -> None:
    for p in list((ROOT / "tools").glob("*.py")) + [ROOT / "host_tests/test_core.py"]:
        try:
            ast.parse(p.read_text(encoding="utf-8"), filename=str(p))
        except SyntaxError as exc:
            fail(f"Python syntax error in {p.relative_to(ROOT)}: {exc}")


def check_windows_scripts() -> None:
    required = {
        "BUILD_RELEASE.bat": ("idf.py set-target esp32c3", "idf.py build", "static_audit.py", "verify_release.py"),
        "FLASH_RELEASE_OTA1.bat": ("read_flash 0x0 0x400000", "write_flash 0x1A0000", "ota_slot.py", "backup"),
        "RECOVERY_ORIGINAL.bat": ("verify_original_dump.py", "write_flash 0x0", "dump1.bin"),
        "ROLLBACK_OTA0.bat": ("ota_slot.py", "ota0"),
    }
    for rel, needles in required.items():
        text = read(rel)
        for needle in needles:
            if needle not in text: fail(f"Windows script {rel} missing: {needle}")
    for rel in ("FLASH_RELEASE_OTA1.bat", "RECOVERY_ORIGINAL.bat", "ROLLBACK_OTA0.bat", "03_MONITOR.bat"):
        if '%PORT%' in read(rel) and '"%PORT%"' not in read(rel):
            fail(f"unquoted COM port reference in {rel}")

    ps = read("FLASH_RELEASE_OTA1.ps1")
    for needle in ("FLASH_RELEASE_OTA1.bat", "CUBE_PORT", "cmd.exe /c"):
        if needle not in ps: fail(f"PowerShell flashing wrapper missing: {needle}")


def main() -> int:
    check_version(); check_cmake(); check_config(); check_source_hygiene(); check_behavioral_hooks()
    check_partitions_and_dump(); check_original_app_images(); check_web(); check_python(); check_windows_scripts()
    if ERRORS:
        for e in ERRORS: print("ERROR:", e)
        print(f"static audit: FAIL ({len(ERRORS)} issue(s))")
        return 1
    print("static audit: PASS")
    print("- C/C++ source/header delimiters and hygiene: PASS")
    print("- CMake/component/assets: PASS")
    print("- ESP32-C3 + Bluedroid + CAN recovery defaults: PASS")
    print("- OTA, partition map and original dump slices: PASS")
    print("- Web UI, local-only policy and OTA progress: PASS")
    print("- Windows scripts and Python syntax: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
