#include "gui/widgets/gui_label.h"

#include "gui/render/gui_renderer.h"

static gui_size_t gui_label_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_label_t *label = (gui_label_t *)widget;
    const int scale = label->scale > 0 ? label->scale : 1;
    gui_size_t size = {
        .width = (int16_t)gui_font_measure_text_width_scaled(label->font, label->text, scale),
        .height = label->font != NULL ? (int16_t)(label->font->line_height * scale) : 0,
    };

    if (widget->frame.width > 0 && !label->text_auto_size) {
        size.width = widget->frame.width;
    } else if (size.width > available.width) {
        size.width = available.width;
    }

    if (widget->frame.height > 0 && !label->text_auto_size) {
        size.height = widget->frame.height;
    }

    return size;
}

static void gui_label_layout(gui_widget_t *widget)
{
    (void)widget;
}

static void gui_label_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_label_t *label = (gui_label_t *)widget;
    if (label->font == NULL || label->text == NULL) {
        return;
    }

    const gui_rect_t absolute = gui_widget_absolute_rect(widget);
    const gui_rect_t clipped = gui_rect_intersect(absolute, clip_rect);
    if (gui_rect_is_empty(clipped)) {
        return;
    }

    const int scale = label->scale > 0 ? label->scale : 1;
    const int text_width = gui_font_measure_text_width_scaled(label->font, label->text, scale);
    const int text_height = label->font->line_height * scale;
    int origin_x = absolute.x;
    if (label->align == GUI_LABEL_ALIGN_CENTER) {
        origin_x = absolute.x + (absolute.width - text_width) / 2;
    } else if (label->align == GUI_LABEL_ALIGN_RIGHT) {
        origin_x = absolute.x + absolute.width - text_width;
    }

    int origin_y = absolute.y + (absolute.height - text_height) / 2;
    if (label->vertical_align == GUI_LABEL_VERTICAL_ALIGN_TOP) {
        origin_y = absolute.y;
    } else if (label->vertical_align == GUI_LABEL_VERTICAL_ALIGN_BOTTOM) {
        origin_y = absolute.y + absolute.height - text_height;
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

    const gui_point_t origin = {
        .x = origin_x,
        .y = origin_y,
    };
    gui_renderer_draw_text_scaled(renderer, origin, label->font, label->text, label->color, scale);

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
    }
}

static bool gui_label_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    (void)widget;
    (void)event;
    return false;
}

static const gui_widget_vtable_t GUI_LABEL_VTABLE = {
    .measure = gui_label_measure,
    .layout = gui_label_layout,
    .paint = gui_label_paint,
    .input = gui_label_input,
};

void gui_label_init(gui_label_t *label, const gui_font_t *font, const char *text, gui_color_t color)
{
    gui_widget_init(&label->base, &GUI_LABEL_VTABLE);
    label->font = font;
    label->text = text;
    label->color = color;
    label->scale = 1;
    label->align = GUI_LABEL_ALIGN_LEFT;
    label->vertical_align = GUI_LABEL_VERTICAL_ALIGN_CENTER;
    label->text_auto_size = true;
}

void gui_label_set_text(gui_label_t *label, const char *text)
{
    if (label->text == text) {
        return;
    }

    gui_widget_invalidate(&label->base);
    label->text = text;
    gui_widget_request_layout(&label->base);
    gui_widget_invalidate(&label->base);
}

void gui_label_set_scale(gui_label_t *label, uint8_t scale)
{
    if (scale == 0) {
        scale = 1;
    }
    if (label->scale == scale) {
        return;
    }
    gui_widget_invalidate(&label->base);
    label->scale = scale;
    gui_widget_request_layout(&label->base);
    gui_widget_invalidate(&label->base);
}

void gui_label_set_align(gui_label_t *label, uint8_t align)
{
    if (label->align == align) {
        return;
    }
    label->align = align;
    gui_widget_invalidate(&label->base);
}

void gui_label_set_vertical_align(gui_label_t *label, uint8_t vertical_align)
{
    if (label->vertical_align == vertical_align) {
        return;
    }
    label->vertical_align = vertical_align;
    gui_widget_invalidate(&label->base);
}

void gui_label_set_text_auto_size(gui_label_t *label, bool enabled)
{
    if (label->text_auto_size == enabled) {
        return;
    }
    label->text_auto_size = enabled;
    gui_widget_request_layout(&label->base);
    gui_widget_invalidate(&label->base);
}
