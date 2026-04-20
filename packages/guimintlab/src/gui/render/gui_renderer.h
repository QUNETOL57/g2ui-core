#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "gui/core/gui_assets.h"
#include "gui/core/gui_types.h"
#include "gui/display/gui_display.h"

typedef struct gui_renderer_t {
    gui_display_t *display;
    gui_rect_t clip_rect;
    gui_color_t *frame_buffer;
} gui_renderer_t;

esp_err_t gui_renderer_init(gui_renderer_t *renderer, gui_display_t *display);
void gui_renderer_set_clip(gui_renderer_t *renderer, gui_rect_t clip_rect);
gui_rect_t gui_renderer_get_clip(const gui_renderer_t *renderer);
esp_err_t gui_renderer_present(gui_renderer_t *renderer, gui_rect_t rect);
esp_err_t gui_renderer_clear(gui_renderer_t *renderer, gui_color_t color);
esp_err_t gui_renderer_fill_rect(gui_renderer_t *renderer, gui_rect_t rect, gui_color_t color);
esp_err_t gui_renderer_stroke_rect(gui_renderer_t *renderer, gui_rect_t rect, int thickness, gui_color_t color);
esp_err_t gui_renderer_draw_text(gui_renderer_t *renderer, gui_point_t origin, const gui_font_t *font, const char *text, gui_color_t color);
esp_err_t gui_renderer_draw_text_scaled(gui_renderer_t *renderer, gui_point_t origin, const gui_font_t *font, const char *text, gui_color_t color, int scale);
esp_err_t gui_renderer_draw_icon(gui_renderer_t *renderer, gui_point_t origin, const gui_icon_asset_t *icon, gui_color_t color);
esp_err_t gui_renderer_draw_image(gui_renderer_t *renderer, gui_point_t origin, const gui_image_asset_t *image);
