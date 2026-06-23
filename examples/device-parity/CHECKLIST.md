# Device parity checklist — canvas `564bf855…6227c`

Compare each item on the **flashed device** against the **G2UI web preview** for project `1` (320×170, RGB565, screen `screen_main`).

Mark each checkbox when the device matches the preview.

1. - [ ] **`screen_main`** (screen) — Full 320×170 canvas; black background (`#000000` / token `bg`); 1 px white outer border; no clipping at edges. *Compare: web preview frame vs device bezel.*

2. - [ ] **`pan_1`** (panel, header) — Transparent container at top (1,1) 318×21; red background defined but `drawBackground: false`, so only child icons are visible. *Compare: no red header bar unless runtime draws it anyway.*

3. - [ ] **`ico_4`** (icon) — Menu icon (`menu`), white, top-left of header at (6,2) 16×16. *Compare: icon glyph and position in header row.*

4. - [ ] **`ico_3`** (icon) — Wi‑Fi icon (`wifi_full`), white, at (264,2) 19×16. *Compare: signal bars glyph near top-right cluster.*

5. - [ ] **`ico_2`** (icon) — Cellular icon (`network_4_bars`), white, at (243,2) 15×16. *Compare: bars icon left of Wi‑Fi.*

6. - [ ] **`ico_1`** (icon) — Battery icon (`battery_100`), white, at (288,2) 24×16. *Compare: full battery glyph at far right of header.*

7. - [ ] **`lab_1`** (label) — Text `temp:`, gray `#9C9C9C`, Spleen 8 px, upper-left at (9,38) 31×8. *Compare: label text, color, and baseline.*

8. - [ ] **`lab_4`** (label) — Text `hum:`, gray `#9C9C9C`, Spleen 8 px, at (10,49) 21×8 below `lab_1`. *Compare: spacing between temp/hum labels.*

9. - [ ] **`fre_2`** (freehand) — White 1 px stroke; curved path in upper-center region, frame (75,11) 137×34. *Compare: curve shape and continuity (no gaps/drops).*

10. - [ ] **`tri_2`** (triangle) — Up-pointing triangle, white fill, 1 px accent border (`#1E90FF`), frame (263,32) 36×32. *Compare: direction, fill, and blue outline.*

11. - [ ] **`pan_4`** (panel, screen) — Transparent center panel (1,53) 318×86; no visible panel chrome, children only. *Compare: center area shows readout/button, not a boxed panel.*

12. - [ ] **`lab_2`** (label) — Large white `89`, Org_01 63 px, centered in panel at (108,0) 109×63 relative to `pan_4`. *Compare: digit size, weight, and horizontal centering.*

13. - [ ] **`lab_3`** (label) — White `g`, Org_01 28 px, at (279,17) 21×28 to the right of the main value. *Compare: unit glyph alignment with `89`.*

14. - [ ] **`but_4`** (button) — Text `TARE`, red `#FF2929` on dark red-brown bg `#573232`, radius 3, at (145,52) 28×11 inside `pan_4`. *Compare: text, colors, rounded rect.*

15. - [ ] **`tri_1`** (triangle) — Up-pointing triangle, white, no accent border, frame (231,78) 36×32 below center readout. *Compare: shape vs `tri_2` (no blue border).*

16. - [ ] **`fre_1`** (freehand) — White 1 px stroke; small closed/curved stroke, frame (14,68) 54×54 left of center. *Compare: small arc/blob matches preview path.*

17. - [ ] **`lin_2`** (line) — Blue `#0400FF` diagonal, stroke 1 px, frame (63,49) 46×67 from upper-left to lower-right. *Compare: angle, color, and endpoints.*

18. - [ ] **`lin_1`** (line) — Green `#00FF1E` horizontal, stroke 1 px, frame (69,129) 134×1. *Compare: straight horizontal rule above bottom bar.*

19. - [ ] **`cir_2`** (circle) — 32×32 at (234,119); 1 px red border `#FF0000`, fill not drawn (`drawBackground: false`). *Compare: red ring/circle outline.*

20. - [ ] **`cir_1`** (circle) — 32×32 at (278,123); default white circle (no explicit border in JSON). *Compare: solid/outline circle to the right of `cir_2`.*

21. - [ ] **`but_5`** (button) — Text `Button` with `earth` icon, white text, BDF 7 px, at (7,138) 80×24 above button bar. *Compare: icon+text layout and padding.*

22. - [ ] **`pan_3`** (panel, button bar) — Transparent footer strip (1,139) 318×30; hosts three square buttons. *Compare: no visible panel background.*

23. - [ ] **`but_1`** (button) — Text `U`, white on black, 1 px orange border `#FF8702`, radius 3, 24×24 at (99,2) in bar. *Compare: left bar button styling.*

24. - [ ] **`but_3`** (button) — Text `T`, same styling as `but_1`, 24×24 at (144,2). *Compare: middle bar button.*

25. - [ ] **`but_2`** (button) — Text `+`, same styling as `but_1`, 24×24 at (190,2). *Compare: right bar button; all three buttons evenly spaced.*
