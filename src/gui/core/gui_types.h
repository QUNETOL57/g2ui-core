#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t gui_color_t;

typedef struct {
    int16_t x;
    int16_t y;
} gui_point_t;

typedef struct {
    int16_t width;
    int16_t height;
} gui_size_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} gui_rect_t;

static inline gui_rect_t gui_rect_make(int x, int y, int width, int height)
{
    gui_rect_t rect = {
        .x = (int16_t)x,
        .y = (int16_t)y,
        .width = (int16_t)width,
        .height = (int16_t)height,
    };
    return rect;
}

static inline bool gui_rect_is_empty(gui_rect_t rect)
{
    return rect.width <= 0 || rect.height <= 0;
}

static inline int16_t gui_rect_right(gui_rect_t rect)
{
    return rect.x + rect.width;
}

static inline int16_t gui_rect_bottom(gui_rect_t rect)
{
    return rect.y + rect.height;
}

static inline bool gui_rect_contains_point(gui_rect_t rect, gui_point_t point)
{
    return !gui_rect_is_empty(rect) &&
           point.x >= rect.x &&
           point.y >= rect.y &&
           point.x < gui_rect_right(rect) &&
           point.y < gui_rect_bottom(rect);
}

static inline gui_rect_t gui_rect_translate(gui_rect_t rect, int dx, int dy)
{
    rect.x += (int16_t)dx;
    rect.y += (int16_t)dy;
    return rect;
}

static inline gui_rect_t gui_rect_intersect(gui_rect_t a, gui_rect_t b)
{
    const int16_t x0 = a.x > b.x ? a.x : b.x;
    const int16_t y0 = a.y > b.y ? a.y : b.y;
    const int16_t x1 = gui_rect_right(a) < gui_rect_right(b) ? gui_rect_right(a) : gui_rect_right(b);
    const int16_t y1 = gui_rect_bottom(a) < gui_rect_bottom(b) ? gui_rect_bottom(a) : gui_rect_bottom(b);

    if (x1 <= x0 || y1 <= y0) {
        return gui_rect_make(0, 0, 0, 0);
    }

    return gui_rect_make(x0, y0, x1 - x0, y1 - y0);
}

static inline gui_rect_t gui_rect_union(gui_rect_t a, gui_rect_t b)
{
    if (gui_rect_is_empty(a)) {
        return b;
    }
    if (gui_rect_is_empty(b)) {
        return a;
    }

    const int16_t x0 = a.x < b.x ? a.x : b.x;
    const int16_t y0 = a.y < b.y ? a.y : b.y;
    const int16_t x1 = gui_rect_right(a) > gui_rect_right(b) ? gui_rect_right(a) : gui_rect_right(b);
    const int16_t y1 = gui_rect_bottom(a) > gui_rect_bottom(b) ? gui_rect_bottom(a) : gui_rect_bottom(b);
    return gui_rect_make(x0, y0, x1 - x0, y1 - y0);
}

static inline bool gui_rect_overlaps(gui_rect_t a, gui_rect_t b)
{
    return !gui_rect_is_empty(gui_rect_intersect(a, b));
}

static inline gui_color_t gui_color_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (gui_color_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}
