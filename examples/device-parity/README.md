Device parity example for `g2ui-core` runtime checks against the G2UI web preview.

## Project

- **Canvas id:** `canvas_564bf855_849b_4c6c_8d5d_4e3714a6227c`
- **Display:** 320×170, RGB565
- **Screen:** `screen_main`

This project contains a full UI layout exported from the G2UI editor: header icons, button bar, center readout, labels, freehand strokes, circles, triangles, and lines.

## Verification checklist

Use [CHECKLIST.md](./CHECKLIST.md) to compare the flashed device against the web preview. Each item is numbered with a checkbox for on-device sign-off.

## Build

1. Open this folder as an ESP-IDF project:
   - `cd examples/device-parity`
2. Set target (example):
   - `idf.py set-target esp32s3`
3. Build:
   - `idf.py build`
4. Flash and monitor:
   - `idf.py -p <PORT> flash monitor`

## Default ST7789 wiring in `app_main.c`

- MOSI: `11`
- SCLK: `12`
- CS: `10`
- DC: `9`
- RST: `8`
- BL: `13`

Adjust pins, orientation flags, and panel size in `main/app_main.c` for your board. The embedded project expects a **320×170** viewport.
