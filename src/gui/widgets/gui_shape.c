#include "gui/widgets/gui_shape.h"

#include "gui/render/gui_renderer.h"

static int visible_line_y(int value, int height, int stroke_width)
{
    const int fallback = height / 2;
    const int pad = stroke_width / 2;
    const int max_y = (height - ((stroke_width + 1) / 2)) > pad
        ? (height - ((stroke_width + 1) / 2))
        : pad;
    int y = value;
    if (height <= 0) {
        return 0;
    }
    if (y < 0) {
        y = fallback;
    }
    if (y < pad) {
        y = pad;
    }
    if (y > max_y) {
        y = max_y;
    }
    return y;
}

static gui_size_t gui_shape_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_size_t size = {
        .width = widget->frame.width > 0 ? widget->frame.width : available.width,
        .height = widget->frame.height > 0 ? widget->frame.height : available.height,
    };
    return size;
}

static void gui_shape_layout(gui_widget_t *widget)
{
    (void)widget;
}

static gui_triangle_direction_t to_renderer_direction(gui_shape_triangle_direction_t direction)
{
    switch (direction) {
        case GUI_SHAPE_TRIANGLE_RIGHT:
            return GUI_TRIANGLE_DIRECTION_RIGHT;
        case GUI_SHAPE_TRIANGLE_DOWN:
            return GUI_TRIANGLE_DIRECTION_DOWN;
        case GUI_SHAPE_TRIANGLE_LEFT:
            return GUI_TRIANGLE_DIRECTION_LEFT;
        case GUI_SHAPE_TRIANGLE_UP:
        default:
            return GUI_TRIANGLE_DIRECTION_UP;
    }
}

static void gui_shape_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_shape_t *shape = (gui_shape_t *)widget;
    const gui_rect_t absolute = gui_widget_absolute_rect(widget);
    if (gui_rect_is_empty(gui_rect_intersect(absolute, clip_rect))) {
        return;
    }

    const int normalized_rotation = ((widget->rotation_degrees % 360) + 360) % 360;
    const bool rotation_enabled = (normalized_rotation % 90) == 0 && normalized_rotation != 0;
    if (rotation_enabled) {
        gui_renderer_push_rotation(
            renderer,
            (gui_renderer_rotation_t)(normalized_rotation / 90),
            (gui_point_t){
                .x = (int16_t)(absolute.x + absolute.width / 2),
                .y = (int16_t)(absolute.y + absolute.height / 2),
            });
    }

    switch (shape->kind) {
        case GUI_SHAPE_KIND_RECT:
            if (shape->radius > 0) {
                gui_renderer_fill_rounded_rect(renderer,
                                               absolute,
                                               shape->radius,
                                               shape->fill_color,
                                               shape->draw_fill,
                                               shape->stroke_color,
                                               shape->stroke_width,
                                               shape->draw_border);
            } else {
                if (shape->draw_fill) {
                    gui_renderer_fill_rect(renderer, absolute, shape->fill_color);
                }
                if (shape->draw_border && shape->stroke_width > 0) {
                    gui_renderer_stroke_rect(renderer, absolute, shape->stroke_width, shape->stroke_color);
                }
            }
            break;
        case GUI_SHAPE_KIND_CIRCLE:
            gui_renderer_fill_ellipse(renderer,
                                      absolute,
                                      shape->radius,
                                      shape->fill_color,
                                      shape->draw_fill,
                                      shape->stroke_color,
                                      shape->stroke_width,
                                      shape->draw_border);
            break;
        case GUI_SHAPE_KIND_TRIANGLE:
            gui_renderer_fill_triangle(renderer,
                                       absolute,
                                       to_renderer_direction(shape->direction),
                                       shape->fill_color,
                                       shape->draw_fill,
                                       shape->stroke_color,
                                       shape->stroke_width,
                                       shape->draw_border);
            break;
        case GUI_SHAPE_KIND_LINE: {
            if (!shape->draw_border) {
                break;
            }
            const int stroke_width = shape->stroke_width > 0 ? shape->stroke_width : 1;
            const int view_width = absolute.width > stroke_width ? absolute.width : stroke_width;
            const int view_height = absolute.height > stroke_width ? absolute.height : stroke_width;
            const int x1 = shape->x1;
            const int x2 = shape->x2;
            const int y1 = visible_line_y(shape->y1, view_height, stroke_width);
            const int y2 = visible_line_y(shape->y2, view_height, stroke_width);
            gui_renderer_draw_line(
                renderer,
                (gui_point_t){ .x = (int16_t)(absolute.x + x1), .y = (int16_t)(absolute.y + y1) },
                (gui_point_t){ .x = (int16_t)(absolute.x + x2), .y = (int16_t)(absolute.y + y2) },
                stroke_width,
                shape->stroke_color);
            break;
        }
        default:
            break;
    }

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
    }
}

static bool gui_shape_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    (void)widget;
    (void)event;
    return false;
}

static const gui_widget_vtable_t GUI_SHAPE_VTABLE = {
    .measure = gui_shape_measure,
    .layout = gui_shape_layout,
    .paint = gui_shape_paint,
    .input = gui_shape_input,
};

void gui_shape_init(gui_shape_t *shape, gui_shape_kind_t kind)
{
    gui_widget_init(&shape->base, &GUI_SHAPE_VTABLE);
    shape->kind = kind;
    shape->direction = GUI_SHAPE_TRIANGLE_UP;
    shape->stroke_width = 1;
    shape->fill_color = 0xFFFFu;
    shape->stroke_color = 0xFFFFu;
    shape->draw_fill = kind != GUI_SHAPE_KIND_LINE;
    shape->draw_border = kind == GUI_SHAPE_KIND_LINE;
}

void gui_shape_set_style(gui_shape_t *shape,
                         gui_color_t fill_color,
                         bool draw_fill,
                         gui_color_t stroke_color,
                         uint8_t stroke_width,
                         bool draw_border)
{
    if (shape == NULL) {
        return;
    }
    shape->fill_color = fill_color;
    shape->stroke_color = stroke_color;
    shape->stroke_width = stroke_width > 0 ? stroke_width : 1;
    shape->draw_fill = draw_fill;
    shape->draw_border = shape->kind == GUI_SHAPE_KIND_LINE
        ? true
        : (draw_border && stroke_width > 0);
    gui_widget_invalidate(&shape->base);
}

void gui_shape_set_radius(gui_shape_t *shape, uint8_t radius)
{
    if (shape == NULL) {
        return;
    }
    shape->radius = radius;
    gui_widget_invalidate(&shape->base);
}

void gui_shape_set_triangle_direction(gui_shape_t *shape, gui_shape_triangle_direction_t direction)
{
    if (shape == NULL) {
        return;
    }
    shape->direction = direction;
    gui_widget_invalidate(&shape->base);
}

void gui_shape_set_line(gui_shape_t *shape, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t stroke_width)
{
    if (shape == NULL) {
        return;
    }
    shape->x1 = x1;
    shape->y1 = y1;
    shape->x2 = x2;
    shape->y2 = y2;
    shape->stroke_width = stroke_width > 0 ? stroke_width : 1;
    shape->draw_border = true;
    gui_widget_invalidate(&shape->base);
}
