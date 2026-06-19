#include "gui/core/gui_assets.h"

#include <string.h>

uint32_t gui_utf8_next(const char **cursor)
{
    if (cursor == NULL || *cursor == NULL) {
        return 0;
    }

    const unsigned char *p = (const unsigned char *)*cursor;
    if (*p == 0) {
        return 0;
    }

    uint32_t cp = 0;
    int extra = 0;
    if (*p < 0x80) {
        cp = *p;
    } else if ((*p & 0xE0) == 0xC0) {
        cp = (uint32_t)(*p & 0x1F);
        extra = 1;
    } else if ((*p & 0xF0) == 0xE0) {
        cp = (uint32_t)(*p & 0x0F);
        extra = 2;
    } else if ((*p & 0xF8) == 0xF0) {
        cp = (uint32_t)(*p & 0x07);
        extra = 3;
    } else {
        /* Invalid lead byte: skip one byte and report as-is to avoid getting stuck. */
        *cursor = (const char *)(p + 1);
        return *p;
    }

    for (int i = 0; i < extra; ++i) {
        unsigned char next = p[1 + i];
        if ((next & 0xC0) != 0x80) {
            /* Truncated multi-byte sequence: bail out. */
            *cursor = (const char *)(p + 1 + i);
            return cp;
        }
        cp = (cp << 6) | (uint32_t)(next & 0x3F);
    }

    *cursor = (const char *)(p + 1 + extra);
    return cp;
}

const gui_glyph_t *gui_font_find_glyph(const gui_font_t *font, uint32_t codepoint)
{
    if (font == NULL) {
        return NULL;
    }

    /* Primary (ASCII) range. */
    if (codepoint >= font->first_codepoint && codepoint <= font->last_codepoint) {
        return &font->glyphs[codepoint - font->first_codepoint];
    }

    /* Optional extended ranges (e.g. Cyrillic). */
    for (uint16_t i = 0; i < font->extra_range_count; ++i) {
        const gui_font_range_t *r = &font->extra_ranges[i];
        if (codepoint >= r->first_codepoint && codepoint <= r->last_codepoint) {
            return &font->glyphs[r->glyph_offset + (codepoint - r->first_codepoint)];
        }
    }

    /* Fallback: fold ASCII lowercase onto uppercase if the font only ships A..Z.
     * Harmless when the font already contains lowercase, because the primary
     * range check above would have hit first. */
    if (codepoint >= 'a' && codepoint <= 'z' && font->last_codepoint < 'a') {
        uint32_t upper = codepoint - ('a' - 'A');
        if (upper >= font->first_codepoint && upper <= font->last_codepoint) {
            return &font->glyphs[upper - font->first_codepoint];
        }
    }

    return NULL;
}

bool gui_glyph_pixel_on(const gui_font_t *font, const gui_glyph_t *glyph, uint8_t x, uint8_t y)
{
    if (font == NULL || glyph == NULL || font->bitmap == NULL || x >= glyph->width || y >= glyph->height) {
        return false;
    }

    const uint8_t bytes_per_row = font->bytes_per_row > 0 ? font->bytes_per_row : (uint8_t)((glyph->width + 7U) / 8U);
    const uint32_t byte_index = glyph->bitmap_offset + (uint32_t)y * bytes_per_row + (uint32_t)x / 8U;
    const uint8_t mask = (uint8_t)(0x80U >> (x % 8U));
    return (font->bitmap[byte_index] & mask) != 0;
}

gui_font_style_t gui_font_parse_style(const char *style)
{
    if (style == NULL || strcmp(style, "regular") == 0) {
        return GUI_FONT_STYLE_REGULAR;
    }
    if (strcmp(style, "bold") == 0) {
        return GUI_FONT_STYLE_BOLD;
    }
    if (strcmp(style, "oblique") == 0 || strcmp(style, "italic") == 0) {
        return GUI_FONT_STYLE_OBLIQUE;
    }
    if (strcmp(style, "boldOblique") == 0 || strcmp(style, "bold-oblique") == 0 ||
        strcmp(style, "boldItalic") == 0 || strcmp(style, "bold-italic") == 0) {
        return GUI_FONT_STYLE_BOLD_OBLIQUE;
    }
    return GUI_FONT_STYLE_REGULAR;
}

int gui_font_measure_text_width(const gui_font_t *font, const char *text)
{
    if (font == NULL || text == NULL) {
        return 0;
    }

    int width = 0;
    const char *cursor = text;
    uint32_t cp;
    while ((cp = gui_utf8_next(&cursor)) != 0) {
        const gui_glyph_t *glyph = gui_font_find_glyph(font, cp);
        width += glyph != NULL ? glyph->advance : font->line_height / 2;
    }
    return width;
}

int gui_font_measure_text_width_scaled(const gui_font_t *font, const char *text, int scale)
{
    if (scale <= 1) {
        return gui_font_measure_text_width(font, text);
    }
    return gui_font_measure_text_width(font, text) * scale;
}
