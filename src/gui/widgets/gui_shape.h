#pragma once

#include "gui/core/gui_widget.h"

typedef enum {
    GUI_SHAPE_KIND_RECT = 0,
    GUI_SHAPE_KIND_CIRCLE,
    GUI_SHAPE_KIND_TRIANGLE,
    GUI_SHAPE_KIND_LINE,
} gui_shape_kind_t;

typedef enum {
    GUI_SHAPE_TRIANGLE_UP = 0,
    GUI_SHAPE_TRIANGLE_RIGHT,
    GUI_SHAPE_TRIANGLE_DOWN,
    GUI_SHAPE_TRIANGLE_LEFT,
} gui_shape_triangle_direction_t;

typedef struct {
    gui_widget_t base;
    gui_shape_kind_t kind;
    gui_shape_triangle_direction_t direction;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    uint8_t radius;
    uint8_t stroke_width;
    gui_color_t fill_color;
    gui_color_t stroke_color;
    bool draw_fill;
    bool draw_border;
} gui_shape_t;

void gui_shape_init(gui_shape_t *shape, gui_shape_kind_t kind);
void gui_shape_set_style(gui_shape_t *shape,
                         gui_color_t fill_color,
                         bool draw_fill,
                         gui_color_t stroke_color,
                         uint8_t stroke_width,
                         bool draw_border);
void gui_shape_set_radius(gui_shape_t *shape, uint8_t radius);
void gui_shape_set_triangle_direction(gui_shape_t *shape, gui_shape_triangle_direction_t direction);
void gui_shape_set_line(gui_shape_t *shape, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t stroke_width);
