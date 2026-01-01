#!/usr/bin/env python3
from __future__ import annotations

import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "RELEASE_MANIFEST_SHA256.txt"
SKIP = {OUT.resolve()}

lines = [
    "FGMDCharge / FD_ROUND V2 Release 2.0.6",
    "SHA-256 manifest of release files (manifest file excluded from its own hash).",
    "",
]
for p in sorted(ROOT.rglob("*")):
    if not p.is_file() or p.resolve() in SKIP:
        continue
    rel = p.relative_to(ROOT).as_posix()
    if any(part in {"build", "managed_components", "__pycache__"} for part in p.parts):
        continue
    h = hashlib.sha256(p.read_bytes()).hexdigest()
    lines.append(f"{h}  {rel}")
OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"manifest written: {OUT}")
