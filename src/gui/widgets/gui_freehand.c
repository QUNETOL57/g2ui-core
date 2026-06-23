#include "gui/widgets/gui_freehand.h"

#include "gui/render/gui_renderer.h"

static gui_size_t gui_freehand_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_size_t size = {
        .width = widget->frame.width > 0 ? widget->frame.width : available.width,
        .height = widget->frame.height > 0 ? widget->frame.height : available.height,
    };
    return size;
}

static void gui_freehand_layout(gui_widget_t *widget)
{
    (void)widget;
}

static void gui_freehand_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_freehand_t *freehand = (gui_freehand_t *)widget;
    if (freehand->points == NULL || freehand->point_count == 0) {
        return;
    }

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

    const int stroke_width = freehand->stroke_width > 0 ? freehand->stroke_width : 1;
    for (uint16_t i = 0; i < freehand->point_count; i++) {
        const gui_point_t point = {
            .x = (int16_t)(absolute.x + freehand->points[i].x),
            .y = (int16_t)(absolute.y + freehand->points[i].y),
        };
        gui_renderer_draw_stroke_point(renderer, point, stroke_width, freehand->color);
    }

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
    }
}

static bool gui_freehand_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    (void)widget;
    (void)event;
    return false;
}

static const gui_widget_vtable_t GUI_FREEHAND_VTABLE = {
    .measure = gui_freehand_measure,
    .layout = gui_freehand_layout,
    .paint = gui_freehand_paint,
    .input = gui_freehand_input,
};

void gui_freehand_init(gui_freehand_t *freehand)
{
    gui_widget_init(&freehand->base, &GUI_FREEHAND_VTABLE);
    freehand->stroke_width = 1;
    freehand->color = 0xFFFFu;
}

void gui_freehand_set_points(gui_freehand_t *freehand, const gui_point_t *points, uint16_t point_count)
{
    if (freehand == NULL) {
        return;
    }
    freehand->points = points;
    freehand->point_count = point_count;
    gui_widget_invalidate(&freehand->base);
}

void gui_freehand_set_style(gui_freehand_t *freehand, gui_color_t color, uint8_t stroke_width)
{
    if (freehand == NULL) {
        return;
    }
    freehand->color = color;
    freehand->stroke_width = stroke_width > 0 ? stroke_width : 1;
    gui_widget_invalidate(&freehand->base);
}
