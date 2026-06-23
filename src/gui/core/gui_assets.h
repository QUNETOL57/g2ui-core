#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gui/core/gui_types.h"

typedef struct {
    uint8_t width;
    uint8_t height;
    int8_t x_offset;
    int8_t y_offset;
    uint8_t advance;
    uint32_t bitmap_offset;
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
    uint8_t  bytes_per_row;
    const uint8_t *bitmap;
    const gui_glyph_t *glyphs;
    /* Optional extra ranges appended after the primary one. */
    const gui_font_range_t *extra_ranges;
    uint16_t extra_range_count;
} gui_font_t;

typedef enum {
    GUI_FONT_STYLE_REGULAR = 0,
    GUI_FONT_STYLE_BOLD,
    GUI_FONT_STYLE_OBLIQUE,
    GUI_FONT_STYLE_BOLD_OBLIQUE,
} gui_font_style_t;

typedef struct {
    const char *id;
    const char *family;
    uint16_t size;
    gui_font_style_t style;
    const gui_font_t *font;
} gui_font_registry_entry_t;

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint32_t *rows;
} gui_icon_asset_t;

typedef struct {
    uint8_t width;
    uint8_t height;
    const gui_color_t *pixels;
} gui_image_asset_t;

const gui_glyph_t *gui_font_find_glyph(const gui_font_t *font, uint32_t codepoint);
const gui_font_registry_entry_t *gui_font_registry(void);
size_t gui_font_registry_count(void);
const gui_font_t *gui_font_find_by_id(const char *id);
const gui_font_t *gui_font_find_face(const char *family, uint16_t size, gui_font_style_t style);
const gui_font_t *gui_font_find_face_nearest(const char *family, uint16_t size, gui_font_style_t style);
const gui_font_t *gui_font_default_face(void);
gui_font_style_t gui_font_parse_style(const char *style);
int gui_font_measure_text_width(const gui_font_t *font, const char *text);
int gui_font_measure_text_width_scaled(const gui_font_t *font, const char *text, int scale);
bool gui_glyph_pixel_on(const gui_font_t *font, const gui_glyph_t *glyph, uint8_t x, uint8_t y);

/*
 * Decode one UTF-8 codepoint from `*cursor` and advance the cursor past it.
 * Returns 0 on end-of-string. Invalid sequences are replaced with U+FFFD-like 0 advance.
 */
uint32_t gui_utf8_next(const char **cursor);
