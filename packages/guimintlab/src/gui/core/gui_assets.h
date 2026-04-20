#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gui/core/gui_types.h"

typedef struct {
    uint8_t width;
    uint8_t height;
    int8_t x_offset;
    int8_t y_offset;
    uint8_t advance;
    const uint8_t *rows;
} gui_glyph_t;

/*
 * One contiguous codepoint range inside a font.
 * `glyph_offset` points at the first glyph of the range inside `gui_font_t.glyphs`.
 * This lets a single font stitch together disjoint scripts (e.g. ASCII + Cyrillic)
 * without paying for the empty Unicode space between them.
 */
typedef struct {
    uint32_t first_codepoint;
    uint32_t last_codepoint;
    uint16_t glyph_offset;
} gui_font_range_t;

typedef struct {
    uint32_t first_codepoint;  /* primary (ASCII) range start */
    uint32_t last_codepoint;   /* primary (ASCII) range end   */
    uint8_t  line_height;
    uint8_t  baseline;
    const gui_glyph_t *glyphs;
    /* Optional extra ranges appended after the primary one. */
    const gui_font_range_t *extra_ranges;
    uint16_t extra_range_count;
} gui_font_t;

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t *rows;
} gui_icon_asset_t;

typedef struct {
    uint8_t width;
    uint8_t height;
    const gui_color_t *pixels;
} gui_image_asset_t;

const gui_glyph_t *gui_font_find_glyph(const gui_font_t *font, uint32_t codepoint);
int gui_font_measure_text_width(const gui_font_t *font, const char *text);
int gui_font_measure_text_width_scaled(const gui_font_t *font, const char *text, int scale);

/*
 * Decode one UTF-8 codepoint from `*cursor` and advance the cursor past it.
 * Returns 0 on end-of-string. Invalid sequences are replaced with U+FFFD-like 0 advance.
 */
uint32_t gui_utf8_next(const char **cursor);
