#include "gui/core/gui_assets.h"

#include <stdlib.h>
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

const gui_font_t *gui_font_default_face(void)
{
    const gui_font_t *default_face = gui_font_find_by_id("default_5x7");
    if (default_face != NULL) {
        return default_face;
    }
    const gui_font_registry_entry_t *registry = gui_font_registry();
    const size_t count = gui_font_registry_count();
    return count > 0 ? registry[0].font : NULL;
}

const gui_font_t *gui_font_find_face_nearest(const char *family, uint16_t size, gui_font_style_t style)
{
    const gui_font_registry_entry_t *registry = gui_font_registry();
    const size_t count = gui_font_registry_count();
    if (registry == NULL || count == 0) {
        return gui_font_default_face();
    }

    if (family == NULL || family[0] == '\0') {
        family = "BDF";
    }
    if (size == 0) {
        size = 7;
    }

    const gui_font_t *nearest_in_style = NULL;
    uint16_t nearest_style_distance = UINT16_MAX;
    const gui_font_t *nearest_in_family = NULL;
    uint16_t nearest_family_distance = UINT16_MAX;
    const gui_font_t *smallest_in_style = NULL;
    uint16_t smallest_style_size = UINT16_MAX;
    const gui_font_t *smallest_in_family = NULL;
    uint16_t smallest_family_size = UINT16_MAX;

    for (size_t i = 0; i < count; ++i) {
        const gui_font_registry_entry_t *entry = &registry[i];
        if (entry->family == NULL || entry->font == NULL || strcmp(entry->family, family) != 0) {
            continue;
        }

        if (entry->style == style) {
            if (entry->size == size) {
                return entry->font;
            }
            const uint16_t distance = (uint16_t)abs((int)entry->size - (int)size);
            if (distance < nearest_style_distance) {
                nearest_style_distance = distance;
                nearest_in_style = entry->font;
            }
            if (entry->size < smallest_style_size) {
                smallest_style_size = entry->size;
                smallest_in_style = entry->font;
            }
        }

        if (entry->size == size && nearest_in_family == NULL) {
            nearest_in_family = entry->font;
            nearest_family_distance = 0;
        } else {
            const uint16_t distance = (uint16_t)abs((int)entry->size - (int)size);
            if (distance < nearest_family_distance) {
                nearest_family_distance = distance;
                nearest_in_family = entry->font;
            }
        }
        if (entry->size < smallest_family_size) {
            smallest_family_size = entry->size;
            smallest_in_family = entry->font;
        }
    }

    if (nearest_in_style != NULL) {
        return nearest_in_style;
    }
    if (smallest_in_style != NULL) {
        return smallest_in_style;
    }
    if (nearest_in_family != NULL) {
        return nearest_in_family;
    }
    if (smallest_in_family != NULL) {
        return smallest_in_family;
    }
    return gui_font_default_face();
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
