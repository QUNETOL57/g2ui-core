#include "gui/render/gui_renderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"

static inline int gui_renderer_stride(const gui_renderer_t *renderer)
{
    return renderer->display->config.width;
}

static inline int gui_round_to_int(double value)
{
    return (int)(value >= 0.0 ? value + 0.5 : value - 0.5);
}

static bool gui_renderer_transform_point(const gui_renderer_t *renderer, int x, int y, int *out_x, int *out_y)
{
    int tx = x;
    int ty = y;

    if (renderer->rotation_enabled) {
        const int cx = renderer->rotation_center.x;
        const int cy = renderer->rotation_center.y;
        switch (renderer->rotation_quadrants & 0x03u) {
            case GUI_RENDERER_ROTATION_90:
                tx = cx - (y - cy);
                ty = cy + (x - cx);
                break;
            case GUI_RENDERER_ROTATION_180:
                tx = cx - (x - cx);
                ty = cy - (y - cy);
                break;
            case GUI_RENDERER_ROTATION_270:
                tx = cx + (y - cy);
                ty = cy - (x - cx);
                break;
            case GUI_RENDERER_ROTATION_0:
            default:
                break;
        }
    }

    const gui_point_t point = {
        .x = (int16_t)tx,
        .y = (int16_t)ty,
    };
    if (!gui_rect_contains_point(renderer->clip_rect, point)) {
        return false;
    }
    if (!gui_rect_contains_point(gui_display_bounds(renderer->display), point)) {
        return false;
    }

    *out_x = tx;
    *out_y = ty;
    return true;
}

static void gui_renderer_plot_pixel(gui_renderer_t *renderer, int x, int y, gui_color_t color)
{
    if (renderer == NULL || renderer->frame_buffer == NULL) {
        return;
    }

    int tx = 0;
    int ty = 0;
    if (!gui_renderer_transform_point(renderer, x, y, &tx, &ty)) {
        return;
    }
    renderer->frame_buffer[(size_t)ty * gui_renderer_stride(renderer) + (size_t)tx] = color;
}

static void gui_renderer_fill_span(gui_renderer_t *renderer, int x, int y, int width, gui_color_t color)
{
    if (renderer == NULL || renderer->frame_buffer == NULL || width <= 0) {
        return;
    }

    if (renderer->rotation_enabled) {
        for (int i = 0; i < width; i++) {
            gui_renderer_plot_pixel(renderer, x + i, y, color);
        }
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

static bool ellipse_scanline(int y, int width, int height, int radius, int *out_x, int *out_width)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    const double rx = fmax(0.5, radius > 0 ? (double)radius : (double)width / 2.0);
    const double ry = fmax(0.5, radius > 0 ? (double)radius : (double)height / 2.0);
    const double cx = ((double)width - 1.0) / 2.0;
    const double cy = ((double)height - 1.0) / 2.0;
    const double normalized_y = ((double)y - cy) / ry;
    const double remaining = 1.0 - normalized_y * normalized_y;
    if (remaining < 0.0) {
        return false;
    }

    const double half_width = rx * sqrt(remaining);
    const int left = (int)fmax(0.0, ceil(cx - half_width));
    const int right = (int)fmin((double)(width - 1), floor(cx + half_width));
    if (right < left) {
        return false;
    }

    *out_x = left;
    *out_width = right - left + 1;
    return true;
}

typedef struct {
    int x;
    int y;
} pixel_point_t;

static bool triangle_scanline(int y,
                              int width,
                              int height,
                              gui_triangle_direction_t direction,
                              int *out_x,
                              int *out_width)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (direction == GUI_TRIANGLE_DIRECTION_UP || direction == GUI_TRIANGLE_DIRECTION_DOWN) {
        const int source_y = (direction == GUI_TRIANGLE_DIRECTION_UP) ? y : (height - 1 - y);
        const double progress = (height <= 1) ? 1.0 : ((double)source_y / (double)(height - 1));
        const int row_width = (int)fmax(1.0, (double)gui_round_to_int(1.0 + progress * (double)(width - 1)));
        const int x = (int)fmax(0.0, floor((double)(width - row_width) / 2.0));
        *out_x = x;
        *out_width = row_width > (width - x) ? (width - x) : row_width;
        return *out_width > 0;
    }

    const double progress = (height <= 1)
        ? 1.0
        : (direction == GUI_TRIANGLE_DIRECTION_RIGHT
              ? (double)y / (double)((height - 1) > 0 ? (height - 1) : 1)
              : 1.0 - (double)y / (double)((height - 1) > 0 ? (height - 1) : 1));
    const int row_width = (int)fmax(1.0, (double)gui_round_to_int((double)width * (1.0 - fabs(progress - 0.5) * 2.0)));
    const int x = direction == GUI_TRIANGLE_DIRECTION_RIGHT ? 0 : width - row_width;
    *out_x = x;
    *out_width = row_width;
    return row_width > 0;
}

static void triangle_vertices(int width,
                              int height,
                              gui_triangle_direction_t direction,
                              pixel_point_t *a,
                              pixel_point_t *b,
                              pixel_point_t *c)
{
    const int max_x = width > 0 ? width - 1 : 0;
    const int max_y = height > 0 ? height - 1 : 0;
    const int center_x = max_x / 2;
    const int center_y = height / 2;

    switch (direction) {
        case GUI_TRIANGLE_DIRECTION_DOWN:
            *a = (pixel_point_t){ .x = 0, .y = 0 };
            *b = (pixel_point_t){ .x = max_x, .y = 0 };
            *c = (pixel_point_t){ .x = center_x, .y = max_y };
            break;
        case GUI_TRIANGLE_DIRECTION_LEFT:
            *a = (pixel_point_t){ .x = max_x, .y = 0 };
            *b = (pixel_point_t){ .x = 0, .y = center_y };
            *c = (pixel_point_t){ .x = max_x, .y = max_y };
            break;
        case GUI_TRIANGLE_DIRECTION_RIGHT:
            *a = (pixel_point_t){ .x = 0, .y = 0 };
            *b = (pixel_point_t){ .x = max_x, .y = center_y };
            *c = (pixel_point_t){ .x = 0, .y = max_y };
            break;
        case GUI_TRIANGLE_DIRECTION_UP:
        default:
            *a = (pixel_point_t){ .x = center_x, .y = 0 };
            *b = (pixel_point_t){ .x = max_x, .y = max_y };
            *c = (pixel_point_t){ .x = 0, .y = max_y };
            break;
    }
}

static int compute_corner_inset(int radius, int row)
{
    int inset = radius;
    for (int x = 0; x < radius; x++) {
        const int dx = radius - x;
        const int dy = radius - row;
        if ((dx * dx) + (dy * dy) <= radius * radius) {
            inset = x;
            break;
        }
    }

    if (row == 0) {
        inset = inset > 0 ? inset - 1 : 0;
    }
    if (row == radius - 1) {
        inset = 0;
    }
    return inset;
}

static int rounded_row_inset(int y, int width, int height, int radius)
{
    int r = radius;
    if (r > width / 2) {
        r = width / 2;
    }
    if (r > height / 2) {
        r = height / 2;
    }
    if (r <= 0) {
        return 0;
    }

    int top_inset = 0;
    if (y < r) {
        top_inset = compute_corner_inset(r, y);
    }

    int bottom_inset = 0;
    const int bottom_start = height - r;
    if (y >= bottom_start) {
        const int mirror_row = (r - 1) - (y - bottom_start);
        bottom_inset = compute_corner_inset(r, mirror_row);
    }

    return top_inset > bottom_inset ? top_inset : bottom_inset;
}

static void inner_fill_scanline(int svg_y,
                                int width,
                                int height,
                                int radius,
                                int border_width,
                                int *out_x,
                                int *out_width)
{
    const int w = width > 0 ? width : 0;
    const int bw = border_width > 0 ? border_width : 0;
    const int inner_w = (w - bw * 2) > 0 ? (w - bw * 2) : 0;
    const int inset = rounded_row_inset(svg_y, w, height, radius);
    *out_x = bw + inset;
    *out_width = inner_w - inset * 2;
    if (*out_width < 0) {
        *out_width = 0;
    }
}

typedef struct {
    gui_renderer_t *renderer;
    gui_color_t color;
    int brush;
    int width;
    int height;
    int offset_x;
    int offset_y;
} triangle_border_ctx_t;

static void triangle_visit_brush(int x, int y, void *user_data)
{
    triangle_border_ctx_t *ctx = (triangle_border_ctx_t *)user_data;
    const int brush_start = -((ctx->brush - 1) / 2);
    const int brush_end = brush_start + ctx->brush - 1;
    for (int dy = brush_start; dy <= brush_end; dy++) {
        for (int dx = brush_start; dx <= brush_end; dx++) {
            const int px = x + dx;
            const int py = y + dy;
            if (px >= 0 && px < ctx->width && py >= 0 && py < ctx->height) {
                gui_renderer_plot_pixel(ctx->renderer, ctx->offset_x + px, ctx->offset_y + py, ctx->color);
            }
        }
    }
}

static void raster_line_visit(int x1,
                              int y1,
                              int x2,
                              int y2,
                              void (*visitor)(int x, int y, void *user_data),
                              void *user_data)
{
    int current_x = x1;
    int current_y = y1;
    const int dx = abs(x2 - x1);
    const int sx = x1 < x2 ? 1 : -1;
    const int dy = -abs(y2 - y1);
    const int sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;

    while (true) {
        visitor(current_x, current_y, user_data);
        if (current_x == x2 && current_y == y2) {
            break;
        }
        const int doubled_error = error * 2;
        if (doubled_error >= dy) {
            error += dy;
            current_x += sx;
        }
        if (doubled_error <= dx) {
            error += dx;
            current_y += sy;
        }
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

void gui_renderer_push_rotation(gui_renderer_t *renderer, gui_renderer_rotation_t rotation, gui_point_t center)
{
    if (renderer == NULL) {
        return;
    }

    renderer->rotation_quadrants = (uint8_t)rotation & 0x03u;
    renderer->rotation_enabled = renderer->rotation_quadrants != GUI_RENDERER_ROTATION_0;
    renderer->rotation_center = center;
}

void gui_renderer_pop_rotation(gui_renderer_t *renderer)
{
    if (renderer == NULL) {
        return;
    }

    renderer->rotation_enabled = false;
    renderer->rotation_quadrants = GUI_RENDERER_ROTATION_0;
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

    if (renderer->rotation_enabled) {
        for (int y = rect.y; y < rect.y + rect.height; y++) {
            for (int x = rect.x; x < rect.x + rect.width; x++) {
                gui_renderer_plot_pixel(renderer, x, y, color);
            }
        }
        return ESP_OK;
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

esp_err_t gui_renderer_draw_point(gui_renderer_t *renderer, gui_point_t point, int stroke_width, gui_color_t color)
{
    if (renderer == NULL || stroke_width <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const int stroke_offset = stroke_width / 2;
    return gui_renderer_fill_rect(
        renderer,
        gui_rect_make(point.x - stroke_offset, point.y - stroke_offset, stroke_width, stroke_width),
        color);
}

esp_err_t gui_renderer_draw_stroke_point(gui_renderer_t *renderer, gui_point_t point, int stroke_width, gui_color_t color)
{
    return gui_renderer_draw_point(renderer, point, stroke_width, color);
}

esp_err_t gui_renderer_draw_line(gui_renderer_t *renderer,
                                 gui_point_t from,
                                 gui_point_t to,
                                 int stroke_width,
                                 gui_color_t color)
{
    if (renderer == NULL || stroke_width <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int current_x = from.x;
    int current_y = from.y;
    const int dx = abs(to.x - from.x);
    const int sx = from.x < to.x ? 1 : -1;
    const int dy = -abs(to.y - from.y);
    const int sy = from.y < to.y ? 1 : -1;
    int error = dx + dy;

    while (true) {
        const gui_point_t point = {
            .x = (int16_t)current_x,
            .y = (int16_t)current_y,
        };
        ESP_RETURN_ON_ERROR(
            gui_renderer_draw_point(renderer, point, stroke_width, color),
            "gui_renderer",
            "line point draw failed");
        if (current_x == to.x && current_y == to.y) {
            break;
        }
        const int doubled_error = error * 2;
        if (doubled_error >= dy) {
            error += dy;
            current_x += sx;
        }
        if (doubled_error <= dx) {
            error += dx;
            current_y += sy;
        }
    }

    return ESP_OK;
}

esp_err_t gui_renderer_fill_ellipse(gui_renderer_t *renderer,
                                    gui_rect_t rect,
                                    int radius,
                                    gui_color_t fill_color,
                                    bool draw_fill,
                                    gui_color_t border_color,
                                    int border_width,
                                    bool draw_border)
{
    if (renderer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (rect.width <= 0 || rect.height <= 0) {
        return ESP_OK;
    }

    const bool has_fill = draw_fill;
    const bool has_border = draw_border && border_width > 0;
    if (!has_fill && !has_border) {
        return ESP_OK;
    }

    const gui_color_t outer_color = has_border ? border_color : fill_color;
    const int inner_w = rect.width - border_width * 2;
    const int inner_h = rect.height - border_width * 2;
    const int inner_radius = radius > 0 ? (radius - border_width > 0 ? radius - border_width : 0) : 0;

    for (int y = 0; y < rect.height; y++) {
        int span_x = 0;
        int span_w = 0;
        if (!ellipse_scanline(y, rect.width, rect.height, radius, &span_x, &span_w)) {
            continue;
        }

        if (!has_border || has_fill) {
            gui_renderer_fill_span(renderer, rect.x + span_x, rect.y + y, span_w, outer_color);
            continue;
        }

        int inner_x = 0;
        int inner_w_span = 0;
        const bool has_inner = y >= border_width &&
                               y < rect.height - border_width &&
                               inner_w > 0 &&
                               inner_h > 0 &&
                               ellipse_scanline(y - border_width, inner_w, inner_h, inner_radius, &inner_x, &inner_w_span);
        if (!has_inner) {
            gui_renderer_fill_span(renderer, rect.x + span_x, rect.y + y, span_w, border_color);
            continue;
        }

        const int inner_start = inner_x + border_width;
        const int inner_end = inner_start + inner_w_span;
        const int outer_end = span_x + span_w;
        if (inner_start > span_x) {
            gui_renderer_fill_span(renderer, rect.x + span_x, rect.y + y, inner_start - span_x, border_color);
        }
        if (inner_end < outer_end) {
            gui_renderer_fill_span(renderer, rect.x + inner_end, rect.y + y, outer_end - inner_end, border_color);
        }
    }

    if (has_border && has_fill && inner_w > 0 && inner_h > 0) {
        for (int y = 0; y < inner_h; y++) {
            int span_x = 0;
            int span_w = 0;
            if (!ellipse_scanline(y, inner_w, inner_h, inner_radius, &span_x, &span_w)) {
                continue;
            }
            gui_renderer_fill_span(renderer,
                                   rect.x + border_width + span_x,
                                   rect.y + border_width + y,
                                   span_w,
                                   fill_color);
        }
    }

    return ESP_OK;
}

esp_err_t gui_renderer_fill_triangle(gui_renderer_t *renderer,
                                     gui_rect_t rect,
                                     gui_triangle_direction_t direction,
                                     gui_color_t fill_color,
                                     bool draw_fill,
                                     gui_color_t border_color,
                                     int border_width,
                                     bool draw_border)
{
    if (renderer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (rect.width <= 0 || rect.height <= 0) {
        return ESP_OK;
    }

    const int bw = border_width > 0 ? border_width : 0;
    const bool has_fill = draw_fill;
    const bool has_border = draw_border && bw > 0;
    const int inner_w = rect.width - bw * 2;
    const int inner_h = rect.height - bw * 2;

    if (has_fill && !has_border) {
        for (int y = 0; y < rect.height; y++) {
            int span_x = 0;
            int span_w = 0;
            if (!triangle_scanline(y, rect.width, rect.height, direction, &span_x, &span_w)) {
                continue;
            }
            gui_renderer_fill_span(renderer, rect.x + span_x, rect.y + y, span_w, fill_color);
        }
    }

    if (has_border && has_fill && inner_w > 0 && inner_h > 0) {
        for (int y = 0; y < inner_h; y++) {
            int span_x = 0;
            int span_w = 0;
            if (!triangle_scanline(y, inner_w, inner_h, direction, &span_x, &span_w)) {
                continue;
            }
            gui_renderer_fill_span(renderer, rect.x + bw + span_x, rect.y + bw + y, span_w, fill_color);
        }
    }

    if (has_border) {
        pixel_point_t a;
        pixel_point_t b;
        pixel_point_t c;
        triangle_vertices(rect.width, rect.height, direction, &a, &b, &c);
        const int join_trim = (int)fmax(1.0, ceil((double)bw / 2.0));

        pixel_point_t edges[3][2];
        switch (direction) {
            case GUI_TRIANGLE_DIRECTION_DOWN: {
                const pixel_point_t left_base = { .x = a.x + join_trim, .y = a.y };
                const pixel_point_t right_base = { .x = b.x - join_trim, .y = b.y };
                edges[0][0] = left_base;
                edges[0][1] = right_base;
                edges[1][0] = right_base;
                edges[1][1] = c;
                edges[2][0] = c;
                edges[2][1] = left_base;
                break;
            }
            case GUI_TRIANGLE_DIRECTION_LEFT:
            case GUI_TRIANGLE_DIRECTION_RIGHT: {
                const pixel_point_t top_base = { .x = a.x, .y = a.y + join_trim };
                const pixel_point_t bottom_base = { .x = c.x, .y = c.y - join_trim };
                edges[0][0] = top_base;
                edges[0][1] = bottom_base;
                edges[1][0] = top_base;
                edges[1][1] = b;
                edges[2][0] = b;
                edges[2][1] = bottom_base;
                break;
            }
            case GUI_TRIANGLE_DIRECTION_UP:
            default: {
                const pixel_point_t right_base = { .x = b.x - join_trim, .y = b.y };
                const pixel_point_t left_base = { .x = c.x + join_trim, .y = c.y };
                edges[0][0] = a;
                edges[0][1] = right_base;
                edges[1][0] = right_base;
                edges[1][1] = left_base;
                edges[2][0] = left_base;
                edges[2][1] = a;
                break;
            }
        }

        triangle_border_ctx_t ctx = {
            .renderer = renderer,
            .color = border_color,
            .brush = bw,
            .width = rect.width,
            .height = rect.height,
            .offset_x = rect.x,
            .offset_y = rect.y,
        };
        for (int i = 0; i < 3; i++) {
            raster_line_visit(edges[i][0].x, edges[i][0].y, edges[i][1].x, edges[i][1].y, triangle_visit_brush, &ctx);
        }
    }

    return ESP_OK;
}

esp_err_t gui_renderer_fill_rounded_rect(gui_renderer_t *renderer,
                                         gui_rect_t rect,
                                         int radius,
                                         gui_color_t fill_color,
                                         bool draw_fill,
                                         gui_color_t border_color,
                                         int border_width,
                                         bool draw_border)
{
    if (renderer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (rect.width <= 0 || rect.height <= 0) {
        return ESP_OK;
    }

    const int w = rect.width > 0 ? rect.width : 0;
    const int h = rect.height > 0 ? rect.height : 0;
    const int bw = border_width > 0 ? border_width : 0;
    const bool has_fill = draw_fill;
    const bool has_border = draw_border && bw > 0;
    const gui_color_t outer_color = has_border ? border_color : fill_color;
    const int inner_w = w - bw * 2;
    const int inner_h = h - bw * 2;
    const int inner_radius = radius - bw > 0 ? radius - bw : 0;

    if (has_fill) {
        for (int y = 0; y < h; y++) {
            const int inset = rounded_row_inset(y, w, h, radius);
            const int row_w = w - inset * 2;
            if (row_w <= 0) {
                continue;
            }
            gui_renderer_fill_span(renderer, rect.x + inset, rect.y + y, row_w, outer_color);
        }
    }

    if (has_border && !has_fill) {
        for (int y = 0; y < h; y++) {
            const int outer_inset = rounded_row_inset(y, w, h, radius);
            const int outer_x = outer_inset;
            const int outer_right = w - outer_inset;
            const int outer_w = outer_right - outer_x;
            if (outer_w <= 0) {
                continue;
            }

            if (inner_w <= 0 || inner_h <= 0 || y < bw || y >= h - bw) {
                gui_renderer_fill_span(renderer, rect.x + outer_x, rect.y + y, outer_w, border_color);
                continue;
            }

            const int inner_y = y - bw;
            const int inner_inset = rounded_row_inset(inner_y, inner_w, inner_h, inner_radius);
            const int inner_x = bw + inner_inset;
            const int inner_right = bw + inner_w - inner_inset;
            const int inner_row_w = inner_right - inner_x;
            if (inner_row_w <= 0) {
                gui_renderer_fill_span(renderer, rect.x + outer_x, rect.y + y, outer_w, border_color);
                continue;
            }

            const int left_w = inner_x - outer_x;
            const int right_w = outer_right - inner_right;
            if (left_w > 0) {
                gui_renderer_fill_span(renderer, rect.x + outer_x, rect.y + y, left_w, border_color);
            }
            if (right_w > 0) {
                gui_renderer_fill_span(renderer, rect.x + inner_right, rect.y + y, right_w, border_color);
            }
        }
    }

    if (has_border && has_fill && inner_w > 0 && inner_h > 0) {
        for (int y = 0; y < inner_h; y++) {
            const int svg_y = bw + y;
            int x = 0;
            int width = 0;
            inner_fill_scanline(svg_y, w, h, radius, bw, &x, &width);
            if (width <= 0) {
                continue;
            }
            gui_renderer_fill_span(renderer, rect.x + x, rect.y + svg_y, width, fill_color);
        }
    }

    return ESP_OK;
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
                int run_start = -1;

                for (int col = 0; col < clipped.width; col++) {
                    const int src_x = (clipped.x - glyph_rect.x + col) / scale;
                    const bool pixel_on = gui_glyph_pixel_on(font, glyph, (uint8_t)src_x, (uint8_t)src_y);

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

    if (renderer->rotation_enabled) {
        for (int row = 0; row < clipped.height; row++) {
            const int src_y = clipped.y - image_rect.y + row;
            for (int col = 0; col < clipped.width; col++) {
                const int src_x = clipped.x - image_rect.x + col;
                gui_renderer_plot_pixel(renderer,
                                        clipped.x + col,
                                        clipped.y + row,
                                        image->pixels[src_y * image->width + src_x]);
            }
        }
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
