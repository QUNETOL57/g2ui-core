#include "gui/widgets/gui_icon.h"

#include "gui/render/gui_renderer.h"

static gui_size_t gui_icon_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_icon_widget_t *icon_widget = (gui_icon_widget_t *)widget;
    const gui_icon_asset_t *icon = icon_widget->icon;
    gui_size_t size = {
        .width = icon != NULL ? icon->width : 0,
        .height = icon != NULL ? icon->height : 0,
    };

    if (widget->frame.width > 0) {
        size.width = widget->frame.width;
    } else if (size.width > available.width) {
        size.width = available.width;
    }

    if (widget->frame.height > 0) {
        size.height = widget->frame.height;
    } else if (size.height > available.height) {
        size.height = available.height;
    }

    return size;
}

static void gui_icon_layout(gui_widget_t *widget)
{
    (void)widget;
}

static void gui_icon_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_icon_widget_t *icon_widget = (gui_icon_widget_t *)widget;
    if (icon_widget->icon == NULL) {
        return;
    }

    const gui_rect_t absolute = gui_widget_absolute_rect(widget);
    const gui_rect_t clipped = gui_rect_intersect(absolute, clip_rect);
    if (gui_rect_is_empty(clipped)) {
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

    const gui_point_t origin = {
        .x = absolute.x + (absolute.width - icon_widget->icon->width) / 2,
        .y = absolute.y + (absolute.height - icon_widget->icon->height) / 2,
    };
    gui_renderer_draw_icon(renderer, origin, icon_widget->icon, icon_widget->color);

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
    }
}

static bool gui_icon_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    (void)widget;
    (void)event;
    return false;
}

static const gui_widget_vtable_t GUI_ICON_VTABLE = {
    .measure = gui_icon_measure,
    .layout = gui_icon_layout,
    .paint = gui_icon_paint,
    .input = gui_icon_input,
};

void gui_icon_widget_init(gui_icon_widget_t *icon_widget, const gui_icon_asset_t *icon, gui_color_t color)
{
    gui_widget_init(&icon_widget->base, &GUI_ICON_VTABLE);
    icon_widget->icon = icon;
    icon_widget->color = color;
}

void gui_icon_widget_set_icon(gui_icon_widget_t *icon_widget, const gui_icon_asset_t *icon)
{
    gui_widget_invalidate(&icon_widget->base);
    icon_widget->icon = icon;
    gui_widget_request_layout(&icon_widget->base);
    gui_widget_invalidate(&icon_widget->base);
}
