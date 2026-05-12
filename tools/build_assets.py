#!/usr/bin/env python3
"""
Build GuiMintLab generated assets.

The font pipeline accepts Adafruit GFX .h files and BDF bitmap fonts, converts
them into a normalized .gmlfont.json manifest, then emits identical C and
TypeScript font data for firmware and Studio preview.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import json
import math
import os
import re
import ssl
import urllib.request
import urllib.error
from pathlib import Path


ADAFRUIT_BASE = "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/Fonts"
BDF_BASE = "https://raw.githubusercontent.com/IT-Studio-Rech/bdf-fonts/main"

ADAFRUIT_FILES = [
    *(f"FreeMono{suffix}{size}pt7b.h" for suffix in ["", "Bold", "Oblique", "BoldOblique"] for size in [9, 12, 18, 24]),
    *(f"FreeSans{suffix}{size}pt7b.h" for suffix in ["", "Bold", "Oblique", "BoldOblique"] for size in [9, 12, 18, 24]),
    *(f"FreeSerif{suffix}{size}pt7b.h" for suffix in ["", "Bold", "Italic", "BoldItalic"] for size in [9, 12, 18, 24]),
    "Org_01.h",
    "Picopixel.h",
    "Tiny3x3a2pt7b.h",
    "TomThumb.h",
]

BDF_FILES = [
    "4x6.bdf",
    "5x7.bdf",
    "5x8.bdf",
    "6x10.bdf",
    "6x12.bdf",
    "7x13.bdf",
    "8x13.bdf",
    "9x15.bdf",
    "10x20.bdf",
    "spleen-5x8.bdf",
    "spleen-8x16.bdf",
    "spleen-12x24.bdf",
    "spleen-16x32.bdf",
]


ICONS: list[tuple[str, int, int, list[int]]] = [
    ("gui_icon_wifi_0", 10, 8, [0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0030, 0x0030, 0x0000]),
    ("gui_icon_wifi_1", 10, 8, [0x0000, 0x0000, 0x0000, 0x0078, 0x0084, 0x0030, 0x0030, 0x0000]),
    ("gui_icon_wifi_2", 10, 8, [0x0000, 0x00FC, 0x0102, 0x0000, 0x0078, 0x0084, 0x0030, 0x0000]),
    ("gui_icon_wifi_3", 10, 8, [0x01FE, 0x0201, 0x0000, 0x00FC, 0x0102, 0x0078, 0x0030, 0x0000]),
    ("gui_icon_battery_0", 12, 8, [0x07F8, 0x0806, 0x0802, 0x0802, 0x0802, 0x0806, 0x07F8, 0x0000]),
    ("gui_icon_battery_1", 12, 8, [0x07F8, 0x0F06, 0x0F02, 0x0F02, 0x0F02, 0x0F06, 0x07F8, 0x0000]),
    ("gui_icon_battery_2", 12, 8, [0x07F8, 0x0FC6, 0x0FC2, 0x0FC2, 0x0FC2, 0x0FC6, 0x07F8, 0x0000]),
    ("gui_icon_battery_3", 12, 8, [0x07F8, 0x0FF6, 0x0FF2, 0x0FF2, 0x0FF2, 0x0FF6, 0x07F8, 0x0000]),
    ("gui_icon_battery_4", 12, 8, [0x07F8, 0x0FFE, 0x0FFE, 0x0FFE, 0x0FFE, 0x0FFE, 0x07F8, 0x0000]),
    ("gui_icon_signal_0", 10, 8, [0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000]),
    ("gui_icon_signal_1", 10, 8, [0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0003, 0x0003, 0x0000]),
    ("gui_icon_signal_2", 10, 8, [0x0000, 0x0000, 0x0000, 0x000C, 0x000C, 0x0003, 0x0003, 0x0000]),
    ("gui_icon_signal_3", 10, 8, [0x0000, 0x0030, 0x0030, 0x000C, 0x000C, 0x0003, 0x0003, 0x0000]),
    ("gui_icon_signal_4", 10, 8, [0x00C0, 0x00C0, 0x0030, 0x0030, 0x000C, 0x000C, 0x0003, 0x0003]),
    ("gui_icon_chevron_left", 8, 8, [0x0010, 0x0030, 0x0060, 0x00C0, 0x00C0, 0x0060, 0x0030, 0x0010]),
    ("gui_icon_chevron_right", 8, 8, [0x0008, 0x000C, 0x0006, 0x0003, 0x0003, 0x0006, 0x000C, 0x0008]),
    ("gui_icon_bolt", 8, 8, [0x0018, 0x0038, 0x0078, 0x0018, 0x0030, 0x0038, 0x0030, 0x0000]),
]


@dataclass
class Glyph:
    codepoint: int
    width: int
    height: int
    x_offset: int
    y_offset: int
    advance: int
    bitmap: list[int]
    bitmap_offset: int = 0


@dataclass
class Face:
    id: str
    symbol: str
    family: str
    size: int
    style: str
    source: str
    line_height: int
    baseline: int
    glyphs: dict[int, Glyph] = field(default_factory=dict)
    bytes_per_row: int = 1
    bitmap: list[int] = field(default_factory=list)


def snake(value: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value)
    value = re.sub(r"[^A-Za-z0-9]+", "_", value)
    return value.strip("_").lower()


def c_symbol(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", value)


def download(url: str, target: Path) -> None:
    if target.exists() and target.stat().st_size > 0:
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    print(f"fetch {url}")
    try:
        with urllib.request.urlopen(url, timeout=30) as r:
            target.write_bytes(r.read())
    except urllib.error.URLError as exc:
        if not isinstance(exc.reason, ssl.SSLCertVerificationError):
            raise
        print("  retry without certificate verification")
        context = ssl._create_unverified_context()
        with urllib.request.urlopen(url, timeout=30, context=context) as r:
            target.write_bytes(r.read())


def byte_values(body: str) -> list[int]:
    values: list[int] = []
    for token in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", re.sub(r"/\*.*?\*/", "", body, flags=re.S)):
        values.append(int(token, 0) & 0xFF)
    return values


def pack_rows(rows: list[list[int]], width: int) -> tuple[list[int], int]:
    bytes_per_row = max(1, (width + 7) // 8)
    packed: list[int] = []
    for row in rows:
        out = [0] * bytes_per_row
        for x, bit in enumerate(row[:width]):
            if bit:
                out[x // 8] |= 0x80 >> (x % 8)
        packed.extend(out)
    return packed, bytes_per_row


def bitstream_to_rows(data: list[int], bit_offset: int, width: int, height: int) -> list[list[int]]:
    rows: list[list[int]] = []
    for y in range(height):
        row: list[int] = []
        for x in range(width):
            bit = bit_offset + y * width + x
            byte = data[bit // 8] if bit // 8 < len(data) else 0
            row.append(1 if byte & (0x80 >> (bit % 8)) else 0)
        rows.append(row)
    return rows


def adafruit_identity(name: str) -> tuple[str, str, int, str]:
    base = name.removesuffix(".h")
    m = re.match(r"^(Free(?:Mono|Sans|Serif))(Bold)?(Oblique|Italic)?(\d+)pt7b$", base)
    if m:
        family, bold, slant, size = m.groups()
        style = "regular"
        if bold and slant:
            style = "boldOblique"
        elif bold:
            style = "bold"
        elif slant:
            style = "oblique"
        face_id = f"{snake(family)}_{snake(style)}_{size}pt"
        return family, style, int(size), face_id
    m = re.search(r"(\d+)", base)
    size = int(m.group(1)) if m else 0
    return base, "regular", size, snake(base)


def parse_adafruit(path: Path) -> Face:
    text = path.read_text(encoding="utf-8")
    bitmap_match = re.search(r"const\s+uint8_t\s+\w+Bitmaps\[\]\s+PROGMEM\s*=\s*\{(.*?)\};", text, re.S)
    glyph_match = re.search(r"const\s+GFXglyph\s+\w+Glyphs\[\]\s+PROGMEM\s*=\s*\{(.*?)\};", text, re.S)
    font_match = re.search(r"const\s+GFXfont\s+\w+\s+PROGMEM\s*=\s*\{.*?,.*?,\s*(0x[0-9A-Fa-f]+|\d+),\s*(0x[0-9A-Fa-f]+|\d+),\s*(\d+)", text, re.S)
    if not bitmap_match or not glyph_match or not font_match:
        raise ValueError(f"cannot parse Adafruit font {path}")

    bitmap = byte_values(bitmap_match.group(1))
    first = int(font_match.group(1), 0)
    y_advance = int(font_match.group(3))
    entries = [
        tuple(int(v, 0) for v in m)
        for m in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}", glyph_match.group(1))
    ]

    family, style, size, face_id = adafruit_identity(path.name)
    baseline = max((-entry[5] for entry in entries if entry[2] > 0), default=0)
    glyphs: dict[int, Glyph] = {}
    line_height = y_advance
    for i, (offset, width, height, advance, x_offset, y_offset) in enumerate(entries):
        cp = first + i
        if width <= 0 or height <= 0:
            packed, bytes_per_row = [], max(1, (width + 7) // 8)
        else:
            rows = bitstream_to_rows(bitmap, offset * 8, width, height)
            packed, bytes_per_row = pack_rows(rows, width)
        top_offset = baseline + y_offset
        line_height = max(line_height, top_offset + height)
        glyphs[cp] = Glyph(cp, width, height, x_offset, top_offset, advance, packed)

    face = Face(face_id, f"gui_font_{face_id}", family, size, style, "adafruit", line_height, baseline, glyphs)
    finalize_face(face)
    return face


def parse_bdf(path: Path) -> Face:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    ascent = descent = None
    glyphs: dict[int, Glyph] = {}
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith("FONT_ASCENT"):
            ascent = int(line.split()[1])
        elif line.startswith("FONT_DESCENT"):
            descent = int(line.split()[1])
        elif line.startswith("STARTCHAR"):
            cp = -1
            advance = 0
            bbx = (0, 0, 0, 0)
            bitmap_rows: list[str] = []
            i += 1
            while i < len(lines) and lines[i].strip() != "ENDCHAR":
                part = lines[i].strip()
                if part.startswith("ENCODING"):
                    cp = int(part.split()[1])
                elif part.startswith("DWIDTH"):
                    advance = int(part.split()[1])
                elif part.startswith("BBX"):
                    p = part.split()
                    bbx = (int(p[1]), int(p[2]), int(p[3]), int(p[4]))
                elif part == "BITMAP":
                    i += 1
                    while i < len(lines) and lines[i].strip() != "ENDCHAR":
                        bitmap_rows.append(lines[i].strip())
                        i += 1
                    break
                i += 1
            if cp >= 0:
                width, height, x_offset, y_offset = bbx
                rows: list[list[int]] = []
                for row_hex in bitmap_rows[:height]:
                    value = int(row_hex or "0", 16)
                    bit_count = max(width, len(row_hex) * 4)
                    rows.append([(value >> (bit_count - 1 - x)) & 1 for x in range(width)])
                packed, _ = pack_rows(rows, width)
                glyphs[cp] = Glyph(cp, width, height, x_offset, 0, advance or width, packed)
        i += 1

    if ascent is None:
        ascent = max((g.height + max(0, g.y_offset) for g in glyphs.values()), default=8)
    if descent is None:
        descent = 0
    line_height = ascent + descent
    for g in glyphs.values():
        # BDF y_offset is relative to baseline; renderer y_offset is from line top.
        original_y_offset = 0
        # Re-parse BBX y offset from packed path by scanning would be expensive; store
        # it in a temporary way by reading again for accuracy.
        (void_y := original_y_offset)
        _ = void_y

    # Second pass for accurate y offsets.
    current_cp = -1
    for line in lines:
        part = line.strip()
        if part.startswith("ENCODING"):
            current_cp = int(part.split()[1])
        elif part.startswith("BBX") and current_cp in glyphs:
            p = part.split()
            height = int(p[2])
            y_offset = int(p[4])
            glyphs[current_cp].y_offset = ascent - y_offset - height

    base = path.stem
    m = re.search(r"(\d+)x(\d+)", base)
    size = int(m.group(2)) if m else line_height
    family = "BDF"
    if base.startswith("spleen-"):
        family = "Spleen"
    face_id = "default_5x7" if base == "5x7" else f"bdf_{snake(base)}"
    symbol = "gui_font_default_5x7" if base == "5x7" else f"gui_font_{face_id}"
    face = Face(face_id, symbol, family, size, "regular", "bdf", line_height, ascent, glyphs)
    finalize_face(face)
    return face


def finalize_face(face: Face) -> None:
    max_bpr = max((max(1, (g.width + 7) // 8) for g in face.glyphs.values()), default=1)
    face.bytes_per_row = max_bpr
    bitmap: list[int] = []
    for cp in sorted(face.glyphs):
        g = face.glyphs[cp]
        old_bpr = max(1, (g.width + 7) // 8)
        expanded: list[int] = []
        for row in range(g.height):
            start = row * old_bpr
            expanded.extend(g.bitmap[start:start + old_bpr])
            expanded.extend([0] * (max_bpr - old_bpr))
        g.bitmap_offset = len(bitmap)
        g.bitmap = expanded
        bitmap.extend(expanded)
    face.bitmap = bitmap


def contiguous_ranges(codepoints: list[int]) -> list[tuple[int, int, int]]:
    if not codepoints:
        return []
    ranges: list[tuple[int, int, int]] = []
    start = prev = codepoints[0]
    offset = 0
    for cp in codepoints[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        ranges.append((start, prev, offset))
        offset += prev - start + 1
        start = prev = cp
    ranges.append((start, prev, offset))
    return ranges


def manifest(face: Face) -> dict:
    return {
        "id": face.id,
        "symbol": face.symbol,
        "family": face.family,
        "size": face.size,
        "style": face.style,
        "source": face.source,
        "lineHeight": face.line_height,
        "baseline": face.baseline,
        "bytesPerRow": face.bytes_per_row,
        "glyphs": [
            {
                "codepoint": cp,
                "width": g.width,
                "height": g.height,
                "xOffset": g.x_offset,
                "yOffset": g.y_offset,
                "advance": g.advance,
                "bitmapOffset": g.bitmap_offset,
                "bitmap": g.bitmap,
            }
            for cp, g in sorted(face.glyphs.items())
        ],
    }


def c_array(values: list[int], width: int = 16) -> str:
    if not values:
        return "0x00"
    chunks = []
    for i in range(0, len(values), width):
        chunks.append(", ".join(f"0x{v:02X}" for v in values[i:i + width]))
    return ",\n    ".join(chunks)


def emit_c(faces: list[Face], out_c: Path, out_h: Path) -> None:
    c: list[str] = [
        "/* Auto-generated by tools/build_assets.py. DO NOT EDIT. */",
        '#include "gui/generated/gui_assets_generated.h"',
        "",
    ]
    h: list[str] = [
        "#pragma once",
        "",
        '#include "gui/core/gui_assets.h"',
        "",
    ]

    for face in faces:
        ranges = contiguous_ranges(sorted(face.glyphs))
        glyph_order = [cp for start, end, _ in ranges for cp in range(start, end + 1)]
        extra = ranges[1:]
        bitmap_sym = f"{face.symbol}_bitmap"
        glyph_sym = f"{face.symbol}_glyphs"
        range_sym = f"{face.symbol}_ranges"
        c.append(f"static const uint8_t {bitmap_sym}[] = {{\n    {c_array(face.bitmap)}\n}};")
        c.append(f"static const gui_glyph_t {glyph_sym}[] = {{")
        for cp in glyph_order:
            g = face.glyphs[cp]
            c.append(
                "    { "
                f".width = {g.width}, .height = {g.height}, .x_offset = {g.x_offset}, .y_offset = {g.y_offset}, "
                f".advance = {g.advance}, .bitmap_offset = {g.bitmap_offset} "
                "},"
            )
        c.append("};")
        if extra:
            c.append(f"static const gui_font_range_t {range_sym}[] = {{")
            for start, end, offset in extra:
                c.append(f"    {{ .first_codepoint = 0x{start:X}, .last_codepoint = 0x{end:X}, .glyph_offset = {offset} }},")
            c.append("};")
            ranges_ref = range_sym
            range_count = f"sizeof({range_sym}) / sizeof({range_sym}[0])"
        else:
            ranges_ref = "NULL"
            range_count = "0"
        primary = ranges[0] if ranges else (0, 0, 0)
        c.append(
            f"const gui_font_t {face.symbol} = {{ "
            f".first_codepoint = 0x{primary[0]:X}, .last_codepoint = 0x{primary[1]:X}, "
            f".line_height = {face.line_height}, .baseline = {face.baseline}, .bytes_per_row = {face.bytes_per_row}, "
            f".bitmap = {bitmap_sym}, .glyphs = {glyph_sym}, "
            f".extra_ranges = {ranges_ref}, .extra_range_count = {range_count} "
            "};"
        )
        c.append("")
        h.append(f"extern const gui_font_t {face.symbol};")

    c.append("static const gui_font_registry_entry_t s_font_registry[] = {")
    for face in faces:
        style = {
            "regular": "GUI_FONT_STYLE_REGULAR",
            "bold": "GUI_FONT_STYLE_BOLD",
            "oblique": "GUI_FONT_STYLE_OBLIQUE",
            "boldOblique": "GUI_FONT_STYLE_BOLD_OBLIQUE",
        }.get(face.style, "GUI_FONT_STYLE_REGULAR")
        c.append(
            f'    {{ .id = "{face.id}", .family = "{face.family}", .size = {face.size}, '
            f".style = {style}, .font = &{face.symbol} }},"
        )
    c.append("};")
    c.append("")
    c.append("const gui_font_registry_entry_t *gui_font_registry(void) { return s_font_registry; }")
    c.append("size_t gui_font_registry_count(void) { return sizeof(s_font_registry) / sizeof(s_font_registry[0]); }")
    c.append("")
    c.append("const gui_font_t *gui_font_find_by_id(const char *id) {")
    c.append("    if (id == NULL) return NULL;")
    c.append("    for (size_t i = 0; i < gui_font_registry_count(); ++i) {")
    c.append("        if (s_font_registry[i].id != NULL && strcmp(s_font_registry[i].id, id) == 0) return s_font_registry[i].font;")
    c.append("    }")
    c.append("    return NULL;")
    c.append("}")
    c.append("")
    c.append("const gui_font_t *gui_font_find_face(const char *family, uint16_t size, gui_font_style_t style) {")
    c.append("    if (family == NULL) return NULL;")
    c.append("    for (size_t i = 0; i < gui_font_registry_count(); ++i) {")
    c.append("        if (s_font_registry[i].family != NULL && strcmp(s_font_registry[i].family, family) == 0 && s_font_registry[i].size == size && s_font_registry[i].style == style) return s_font_registry[i].font;")
    c.append("    }")
    c.append("    return NULL;")
    c.append("}")
    c.append("")

    for name, w, hgt, rows in ICONS:
        c.append(f"static const uint16_t {name}_rows[] = {{{', '.join(f'0x{r:04X}' for r in rows)}}};")
        c.append(f"const gui_icon_asset_t {name} = {{ .width = {w}, .height = {hgt}, .rows = {name}_rows }};")
        c.append("")
        h.append(f"extern const gui_icon_asset_t {name};")

    h.extend([
        "",
        "const gui_font_registry_entry_t *gui_font_registry(void);",
        "size_t gui_font_registry_count(void);",
        "const gui_font_t *gui_font_find_by_id(const char *id);",
        "const gui_font_t *gui_font_find_face(const char *family, uint16_t size, gui_font_style_t style);",
        "",
    ])

    # strcmp is used by generated lookup helpers.
    c.insert(2, "#include <string.h>")
    c.insert(3, "")
    out_c.write_text("\n".join(c) + "\n", encoding="utf-8")
    out_h.write_text("\n".join(h) + "\n", encoding="utf-8")


def emit_ts(faces: list[Face], out_ts: Path) -> None:
    out_ts.parent.mkdir(parents=True, exist_ok=True)
    data = []
    for face in faces:
        ranges = contiguous_ranges(sorted(face.glyphs))
        glyphs = []
        for start, end, offset in ranges:
            for cp in range(start, end + 1):
                g = face.glyphs[cp]
                glyphs.append({
                    "codepoint": cp,
                    "width": g.width,
                    "height": g.height,
                    "xOffset": g.x_offset,
                    "yOffset": g.y_offset,
                    "advance": g.advance,
                    "bitmapOffset": g.bitmap_offset,
                })
        data.append({
            "id": face.id,
            "family": face.family,
            "size": face.size,
            "style": face.style,
            "lineHeight": face.line_height,
            "baseline": face.baseline,
            "bytesPerRow": face.bytes_per_row,
            "bitmap": face.bitmap,
            "glyphs": glyphs,
            "ranges": [{"first": a, "last": b, "glyphOffset": c} for a, b, c in ranges],
        })
    out_ts.write_text(
        "/* Auto-generated by guimintlab-core/tools/build_assets.py. DO NOT EDIT. */\n"
        "import type { BitmapFontFace } from \"../fontTypes\";\n\n"
        f"export const FONT_FACES: BitmapFontFace[] = {json.dumps(data, separators=(',', ':'))};\n",
        encoding="utf-8",
    )


def build_faces(root: Path) -> list[Face]:
    upstream = root / "assets-src" / "fonts" / "upstream"
    faces: list[Face] = []
    for name in ADAFRUIT_FILES:
        target = upstream / "adafruit" / name
        download(f"{ADAFRUIT_BASE}/{name}", target)
        faces.append(parse_adafruit(target))
    for name in BDF_FILES:
        target = upstream / "bdf" / name
        download(f"{BDF_BASE}/{urllib.request.pathname2url(name)}", target)
        faces.append(parse_bdf(target))
    return sorted(faces, key=lambda f: (f.family, f.style, f.size, f.id))


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    faces = build_faces(root)
    manifest_dir = root / "assets-src" / "fonts" / "generated"
    manifest_dir.mkdir(parents=True, exist_ok=True)
    for face in faces:
        (manifest_dir / f"{face.id}.gmlfont.json").write_text(
            json.dumps(manifest(face), indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    generated = root / "src" / "gui" / "generated"
    emit_c(faces, generated / "gui_assets_generated.c", generated / "gui_assets_generated.h")

    studio = root.parent / "guimintlab-studio"
    if studio.exists():
        emit_ts(faces, studio / "src" / "features" / "fonts" / "generated" / "fontAssets.ts")

    print(f"generated {len(faces)} font faces")


if __name__ == "__main__":
    main()
