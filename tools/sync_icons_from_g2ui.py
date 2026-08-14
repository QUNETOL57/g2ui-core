#!/usr/bin/env python3
"""Sync the firmware icon catalog from the g2ui editor libraries."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ICON_PATTERN = re.compile(
    r'\{\s*id:\s*"([^"]+)"\s*,\s*group:\s*"[^"]*"\s*,\s*width:\s*(\d+)\s*,\s*height:\s*(\d+)\s*,\s*rows:\s*\[([^\]]+)\]\s*\}',
    re.S,
)


def parse_icons(path: Path) -> list[dict]:
    text = path.read_text(encoding="utf-8")
    icons: list[dict] = []
    for match in ICON_PATTERN.finditer(text):
        icon_id = match.group(1)
        width = int(match.group(2))
        height = int(match.group(3))
        rows = [int(token, 0) for token in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", match.group(4))]
        if len(rows) != height:
            raise SystemExit(f"{path}: {icon_id} expected {height} rows, got {len(rows)}")
        if width > 32:
            raise SystemExit(f"{path}: {icon_id} width {width} exceeds uint32 row storage")
        icons.append({"id": icon_id, "width": width, "height": height, "rows": rows})
    return icons


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--g2ui",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "g2ui",
        help="Path to the g2ui editor repository",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sources = [
        args.g2ui / "apps/web/src/entities/icon/iconLibraryData.ts",
        args.g2ui / "apps/web/src/entities/icon/pixelarticonsLibraryData.ts",
    ]
    icons: list[dict] = []
    seen: set[str] = set()
    for source in sources:
        if not source.exists():
            raise SystemExit(f"missing icon source: {source}")
        for icon in parse_icons(source):
            if icon["id"] in seen:
                continue
            seen.add(icon["id"])
            icons.append(icon)

    out = root / "assets-src" / "icons" / "library.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(icons, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"wrote {len(icons)} icons to {out}")


if __name__ == "__main__":
    main()
