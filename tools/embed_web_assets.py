#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "main/web_assets.c"
ASSETS = (
    ("web_index_html", ROOT / "main/web/index.html"),
    ("web_style_css", ROOT / "main/web/style.css"),
    ("web_app_js", ROOT / "main/web/app.js"),
)


def emit_array(name: str, data: bytes) -> str:
    # Keep a trailing NUL so the exact payload can also be used as a C string,
    # while the exported *_len excludes that sentinel.
    payload = data + b"\0"
    lines = []
    for i in range(0, len(payload), 16):
        chunk = payload[i:i + 16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk))
    return (f"const uint8_t {name}[] = {{\n" + ",\n".join(lines) +
            f"\n}};\nconst size_t {name}_len = sizeof({name}) - 1U;\n")


def main() -> int:
    blocks: list[str] = [
        "/* Auto-generated from main/web assets. Do not edit manually. */",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
    ]
    for name, path in ASSETS:
        if not path.is_file():
            raise SystemExit(f"missing asset: {path}")
        blocks.append(emit_array(name, path.read_bytes()))
    OUT.write_text("\n".join(blocks), encoding="utf-8", newline="\n")
    print(f"generated {OUT.relative_to(ROOT)}")
    for name, path in ASSETS:
        print(f"  {path.relative_to(ROOT)}: {path.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
