#include "gui/render/gui_renderer.h"

#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"

static inline int gui_renderer_stride(const gui_renderer_t *renderer)
{
    return renderer->display->config.width;
}

static void gui_renderer_fill_span(gui_renderer_t *renderer, int x, int y, int width, gui_color_t color)
{
    if (renderer == NULL || renderer->frame_buffer == NULL || width <= 0) {
        return;
    }

    const gui_rect_t span = gui_rect_intersect(gui_rect_make(x, y, width, 1), renderer->clip_rect);
    if (gui_rect_is_empty(span)) {
        return;
    }

    gui_color_t *dst = renderer->frame_buffer + (size_t)span.y * gui_renderer_stride(renderer) + span.x;
    for (int i = 0; i < span.width; i++) {
        dst[i] = color;
    }
}

esp_err_t gui_renderer_init(gui_renderer_t *renderer, gui_display_t *display)
{
    if (renderer == NULL || display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(renderer, 0, sizeof(*renderer));
    renderer->display = display;
    renderer->clip_rect = gui_display_bounds(display);
    const size_t frame_pixels = (size_t)display->config.width * display->config.height;
    renderer->frame_buffer = heap_caps_malloc(frame_pixels * sizeof(gui_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (renderer->frame_buffer == NULL) {
        renderer->frame_buffer = heap_caps_malloc(frame_pixels * sizeof(gui_color_t), MALLOC_CAP_DEFAULT);
    }
    return renderer->frame_buffer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void gui_renderer_set_clip(gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    if (renderer == NULL) {
        return;
    }

    renderer->clip_rect = gui_rect_intersect(clip_rect, gui_display_bounds(renderer->display));
}

gui_rect_t gui_renderer_get_clip(const gui_renderer_t *renderer)
{
    return renderer != NULL ? renderer->clip_rect : gui_rect_make(0, 0, 0, 0);
}

esp_err_t gui_renderer_present(gui_renderer_t *renderer, gui_rect_t rect)
{
    if (renderer == NULL || renderer->frame_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const gui_rect_t clipped = gui_rect_intersect(rect, gui_display_bounds(renderer->display));
    if (gui_rect_is_empty(clipped)) {
        return ESP_OK;
    }

    const gui_color_t *src = renderer->frame_buffer + (size_t)clipped.y * gui_renderer_stride(renderer) + clipped.x;
    return gui_display_flush_rect(renderer->display, clipped, src, gui_renderer_stride(renderer));
}

esp_err_t gui_renderer_clear(gui_renderer_t *renderer, gui_color_t color)
{
    if (renderer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return gui_renderer_fill_rect(renderer, gui_display_bounds(renderer->display), color);
}

esp_err_t gui_renderer_fill_rect(gui_renderer_t *renderer, gui_rect_t rect, gui_color_t color)
{
    if (renderer == NULL || renderer->frame_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const gui_rect_t clipped = gui_rect_intersect(rect, renderer->clip_rect);
    if (gui_rect_is_empty(clipped)) {
        return ESP_OK;
    }

    for (int y = clipped.y; y < (clipped.y + clipped.height); y++) {
        gui_color_t *dst = renderer->frame_buffer + (size_t)y * gui_renderer_stride(renderer) + clipped.x;
        for (int x = 0; x < clipped.width; x++) {
            dst[x] = color;
        }
    }

    return ESP_OK;
}

esp_err_t gui_renderer_stroke_rect(gui_renderer_t *renderer, gui_rect_t rect, int thickness, gui_color_t color)
{
    if (renderer == NULL || thickness <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(gui_renderer_fill_rect(renderer, gui_rect_make(rect.x, rect.y, rect.width, thickness), color), "gui_renderer", "stroke top failed");
    ESP_RETURN_ON_ERROR(gui_renderer_fill_rect(renderer, gui_rect_make(rect.x, rect.y + rect.height - thickness, rect.width, thickness), color), "gui_renderer", "stroke bottom failed");
    ESP_RETURN_ON_ERROR(gui_renderer_fill_rect(renderer, gui_rect_make(rect.x, rect.y, thickness, rect.height), color), "gui_renderer", "stroke left failed");
    return gui_renderer_fill_rect(renderer, gui_rect_make(rect.x + rect.width - thickness, rect.y, thickness, rect.height), color);
}

esp_err_t gui_renderer_draw_text(gui_renderer_t *renderer, gui_point_t origin, const gui_font_t *font, const char *text, gui_color_t color)
{
    return gui_renderer_draw_text_scaled(renderer, origin, font, text, color, 1);
}

esp_err_t gui_renderer_draw_text_scaled(gui_renderer_t *renderer, gui_point_t origin, const gui_font_t *font, const char *text, gui_color_t color, int scale)
{
    if (renderer == NULL || font == NULL || text == NULL || renderer->frame_buffer == NULL || scale <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int pen_x = origin.x;
    const char *cursor = text;
    uint32_t codepoint;
    while ((codepoint = gui_utf8_next(&cursor)) != 0) {
        const gui_glyph_t *glyph = gui_font_find_glyph(font, codepoint);
        if (glyph == NULL) {
            pen_x += (font->line_height / 2) * scale;
            continue;
        }

        const gui_rect_t glyph_rect = gui_rect_make(
            pen_x + glyph->x_offset * scale,
            origin.y + glyph->y_offset * scale,
            glyph->width * scale,
            glyph->height * scale);
        const gui_rect_t clipped = gui_rect_intersect(glyph_rect, renderer->clip_rect);
        if (!gui_rect_is_empty(clipped)) {
            for (int row = 0; row < clipped.height; row++) {
                const int src_y = (clipped.y - glyph_rect.y + row) / scale;
                const uint8_t row_bits = glyph->rows[src_y];
                int run_start = -1;

                for (int col = 0; col < clipped.width; col++) {
                    const int src_x = (clipped.x - glyph_rect.x + col) / scale;
                    const bool pixel_on = (row_bits & (uint8_t)(1U << (glyph->width - 1 - src_x))) != 0;

                    if (pixel_on && run_start < 0) {
                        run_start = col;
                    } else if (!pixel_on && run_start >= 0) {
                        gui_renderer_fill_span(renderer, clipped.x + run_start, clipped.y + row, col - run_start, color);
                        run_start = -1;
                    }
                }

                if (run_start >= 0) {
                    gui_renderer_fill_span(renderer, clipped.x + run_start, clipped.y + row, clipped.width - run_start, color);
                }
            }
        }

        pen_x += glyph->advance * scale;
    }

    return ESP_OK;
}

esp_err_t gui_renderer_draw_icon(gui_renderer_t *renderer, gui_point_t origin, const gui_icon_asset_t *icon, gui_color_t color)
{
    if (renderer == NULL || icon == NULL || renderer->frame_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const gui_rect_t icon_rect = gui_rect_make(origin.x, origin.y, icon->width, icon->height);
    const gui_rect_t clipped = gui_rect_intersect(icon_rect, renderer->clip_rect);
    if (gui_rect_is_empty(clipped)) {
        return ESP_OK;
    }

    for (int row = 0; row < clipped.height; row++) {
        const int src_y = clipped.y - icon_rect.y + row;
        const uint32_t row_bits = icon->rows[src_y];
        int run_start = -1;

        for (int col = 0; col < clipped.width; col++) {
            const int src_x = clipped.x - icon_rect.x + col;
            const bool pixel_on = (row_bits & (uint32_t)(1UL << (icon->width - 1 - src_x))) != 0;
            if (pixel_on && run_start < 0) {
                run_start = col;
            } else if (!pixel_on && run_start >= 0) {
                gui_renderer_fill_span(renderer, clipped.x + run_start, clipped.y + row, col - run_start, color);
                run_start = -1;
            }
        }

        if (run_start >= 0) {
            gui_renderer_fill_span(renderer, clipped.x + run_start, clipped.y + row, clipped.width - run_start, color);
        }
    }

    return ESP_OK;
}

esp_err_t gui_renderer_draw_image(gui_renderer_t *renderer, gui_point_t origin, const gui_image_asset_t *image)
{
    if (renderer == NULL || image == NULL || image->pixels == NULL || renderer->frame_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const gui_rect_t image_rect = gui_rect_make(origin.x, origin.y, image->width, image->height);
    const gui_rect_t clipped = gui_rect_intersect(image_rect, renderer->clip_rect);
    if (gui_rect_is_empty(clipped)) {
        return ESP_OK;
    }

    for (int row = 0; row < clipped.height; row++) {
        const int src_y = clipped.y - image_rect.y + row;
        const gui_color_t *src = &image->pixels[src_y * image->width + (clipped.x - image_rect.x)];
        gui_color_t *dst = renderer->frame_buffer + (size_t)(clipped.y + row) * gui_renderer_stride(renderer) + clipped.x;
        memcpy(dst, src, (size_t)clipped.width * sizeof(gui_color_t));
    }

    return ESP_OK;
}
