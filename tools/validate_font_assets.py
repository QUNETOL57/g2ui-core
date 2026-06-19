#!/usr/bin/env python3
"""Smoke-check generated GuiMintLab font manifests."""

from __future__ import annotations

import json
from pathlib import Path


REQUIRED_FACES = {
    ("FreeMono", "regular"): {9, 12, 18, 24},
    ("FreeMono", "bold"): {9, 12, 18, 24},
    ("FreeMono", "oblique"): {9, 12, 18, 24},
    ("FreeMono", "boldOblique"): {9, 12, 18, 24},
}


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    manifest_dir = root / "assets-src" / "fonts" / "generated"
    manifests = sorted(manifest_dir.glob("*.gmlfont.json"))
    if not manifests:
        raise SystemExit("no generated font manifests found")

    grouped: dict[tuple[str, str], set[int]] = {}
    for path in manifests:
        data = json.loads(path.read_text(encoding="utf-8"))
        glyphs = data.get("glyphs", [])
        if not glyphs:
            raise SystemExit(f"{path.name}: no glyphs")
        bytes_per_row = int(data["bytesPerRow"])
        for glyph in glyphs:
            width = int(glyph["width"])
            height = int(glyph["height"])
            bitmap = glyph["bitmap"]
            expected_len = height * bytes_per_row
            if len(bitmap) != expected_len:
                raise SystemExit(f"{path.name}: U+{glyph['codepoint']:04X} bitmap len {len(bitmap)} != {expected_len}")
            if bytes_per_row < max(1, (width + 7) // 8):
                raise SystemExit(f"{path.name}: bytesPerRow too small for U+{glyph['codepoint']:04X}")
            if int(glyph["advance"]) < 0:
                raise SystemExit(f"{path.name}: negative advance for U+{glyph['codepoint']:04X}")
        key = (data["family"], data["style"])
        grouped.setdefault(key, set()).add(int(data["size"]))

    for key, sizes in REQUIRED_FACES.items():
        missing = sizes - grouped.get(key, set())
        if missing:
            raise SystemExit(f"missing {key[0]} {key[1]} sizes: {sorted(missing)}")

    print(f"validated {len(manifests)} generated font manifests")


if __name__ == "__main__":
    main()
