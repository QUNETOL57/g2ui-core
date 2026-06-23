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
    bool rotation_enabled;
    uint8_t rotation_quadrants;
    gui_point_t rotation_center;
} gui_renderer_t;

typedef enum {
    GUI_RENDERER_ROTATION_0 = 0,
    GUI_RENDERER_ROTATION_90 = 1,
    GUI_RENDERER_ROTATION_180 = 2,
    GUI_RENDERER_ROTATION_270 = 3,
} gui_renderer_rotation_t;

typedef enum {
    GUI_TRIANGLE_DIRECTION_UP = 0,
    GUI_TRIANGLE_DIRECTION_RIGHT,
    GUI_TRIANGLE_DIRECTION_DOWN,
    GUI_TRIANGLE_DIRECTION_LEFT,
} gui_triangle_direction_t;

esp_err_t gui_renderer_init(gui_renderer_t *renderer, gui_display_t *display);
void gui_renderer_set_clip(gui_renderer_t *renderer, gui_rect_t clip_rect);
gui_rect_t gui_renderer_get_clip(const gui_renderer_t *renderer);
void gui_renderer_push_rotation(gui_renderer_t *renderer, gui_renderer_rotation_t rotation, gui_point_t center);
void gui_renderer_pop_rotation(gui_renderer_t *renderer);
esp_err_t gui_renderer_present(gui_renderer_t *renderer, gui_rect_t rect);
esp_err_t gui_renderer_clear(gui_renderer_t *renderer, gui_color_t color);
esp_err_t gui_renderer_fill_rect(gui_renderer_t *renderer, gui_rect_t rect, gui_color_t color);
esp_err_t gui_renderer_stroke_rect(gui_renderer_t *renderer, gui_rect_t rect, int thickness, gui_color_t color);
esp_err_t gui_renderer_draw_point(gui_renderer_t *renderer, gui_point_t point, int stroke_width, gui_color_t color);
esp_err_t gui_renderer_draw_stroke_point(gui_renderer_t *renderer, gui_point_t point, int stroke_width, gui_color_t color);
esp_err_t gui_renderer_draw_line(gui_renderer_t *renderer,
                                 gui_point_t from,
                                 gui_point_t to,
                                 int stroke_width,
                                 gui_color_t color);
esp_err_t gui_renderer_fill_ellipse(gui_renderer_t *renderer,
                                    gui_rect_t rect,
                                    int radius,
                                    gui_color_t fill_color,
                                    bool draw_fill,
                                    gui_color_t border_color,
                                    int border_width,
                                    bool draw_border);
esp_err_t gui_renderer_fill_triangle(gui_renderer_t *renderer,
                                     gui_rect_t rect,
                                     gui_triangle_direction_t direction,
                                     gui_color_t fill_color,
                                     bool draw_fill,
                                     gui_color_t border_color,
                                     int border_width,
                                     bool draw_border);
esp_err_t gui_renderer_fill_rounded_rect(gui_renderer_t *renderer,
                                         gui_rect_t rect,
                                         int radius,
                                         gui_color_t fill_color,
                                         bool draw_fill,
                                         gui_color_t border_color,
                                         int border_width,
                                         bool draw_border);
esp_err_t gui_renderer_draw_text(gui_renderer_t *renderer, gui_point_t origin, const gui_font_t *font, const char *text, gui_color_t color);
esp_err_t gui_renderer_draw_text_scaled(gui_renderer_t *renderer, gui_point_t origin, const gui_font_t *font, const char *text, gui_color_t color, int scale);
esp_err_t gui_renderer_draw_icon(gui_renderer_t *renderer, gui_point_t origin, const gui_icon_asset_t *icon, gui_color_t color);
esp_err_t gui_renderer_draw_image(gui_renderer_t *renderer, gui_point_t origin, const gui_image_asset_t *image);
