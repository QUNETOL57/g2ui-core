#include "gui/widgets/gui_panel.h"

#include "gui/render/gui_renderer.h"

static gui_size_t gui_panel_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_panel_t *panel = (gui_panel_t *)widget;
    int width = widget->frame.width > 0 ? widget->frame.width : available.width;
    int height = widget->frame.height > 0 ? widget->frame.height : available.height;

    if (panel->layout == GUI_LAYOUT_ROW || panel->layout == GUI_LAYOUT_COLUMN) {
        int content_primary = 0;
        int content_cross = 0;
        int child_count = 0;
        const gui_size_t child_available = {
            .width = available.width - (int16_t)(panel->padding * 2),
            .height = available.height - (int16_t)(panel->padding * 2),
        };

        for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
            const gui_size_t child_size = child->vtable != NULL && child->vtable->measure != NULL
                ? child->vtable->measure(child, child_available)
                : child_available;
            if (panel->layout == GUI_LAYOUT_ROW) {
                content_primary += child_size.width;
                if (child_size.height > content_cross) {
                    content_cross = child_size.height;
                }
            } else {
                content_primary += child_size.height;
                if (child_size.width > content_cross) {
                    content_cross = child_size.width;
                }
            }
            child_count++;
        }

        if (child_count > 1) {
            content_primary += (child_count - 1) * panel->gap;
        }

        if (widget->frame.width <= 0) {
            width = panel->layout == GUI_LAYOUT_ROW
                ? content_primary + panel->padding * 2
                : content_cross + panel->padding * 2;
        }
        if (widget->frame.height <= 0) {
            height = panel->layout == GUI_LAYOUT_ROW
                ? content_cross + panel->padding * 2
                : content_primary + panel->padding * 2;
        }
    }

    return (gui_size_t) {
        .width = (int16_t)width,
        .height = (int16_t)height,
    };
}

static void gui_panel_layout(gui_widget_t *widget)
{
    gui_panel_t *panel = (gui_panel_t *)widget;
    if (panel->layout == GUI_LAYOUT_NONE) {
        return;
    }

    const int16_t inner_x = panel->padding;
    const int16_t inner_y = panel->padding;
    const int16_t inner_w = widget->frame.width - (int16_t)(panel->padding * 2);
    const int16_t inner_h = widget->frame.height - (int16_t)(panel->padding * 2);

    int16_t cursor = 0;
    for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        const gui_size_t available = { .width = inner_w, .height = inner_h };
        const gui_size_t size = child->vtable != NULL && child->vtable->measure != NULL
            ? child->vtable->measure(child, available)
            : available;

        gui_rect_t frame = child->frame;
        if (panel->layout == GUI_LAYOUT_ROW) {
            frame.x = inner_x + cursor;
            frame.y = inner_y;
            frame.width = frame.width > 0 ? frame.width : size.width;
            frame.height = frame.height > 0 ? frame.height : inner_h;
            cursor += frame.width + panel->gap;
        } else {
            frame.x = inner_x;
            frame.y = inner_y + cursor;
            frame.width = frame.width > 0 ? frame.width : inner_w;
            frame.height = frame.height > 0 ? frame.height : size.height;
            cursor += frame.height + panel->gap;
        }
        child->frame = frame;
    }
}

static void gui_panel_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_panel_t *panel = (gui_panel_t *)widget;
    const gui_rect_t rect = gui_widget_absolute_rect(widget);
    if (gui_rect_is_empty(gui_rect_intersect(rect, clip_rect))) {
        return;
    }

    if (panel->draw_background) {
        gui_renderer_fill_rect(renderer, rect, panel->background);
    }
    if (panel->draw_border && panel->border_width > 0) {
        gui_renderer_stroke_rect(renderer, rect, panel->border_width, panel->border_color);
    }
}

static bool gui_panel_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    (void)widget;
    (void)event;
    return false;
}

static const gui_widget_vtable_t GUI_PANEL_VTABLE = {
    .measure = gui_panel_measure,
    .layout = gui_panel_layout,
    .paint = gui_panel_paint,
    .input = gui_panel_input,
};

void gui_panel_init(gui_panel_t *panel)
{
    gui_widget_init(&panel->base, &GUI_PANEL_VTABLE);
    panel->layout = GUI_LAYOUT_NONE;
}

void gui_panel_set_layout(gui_panel_t *panel, gui_layout_t layout)
{
    panel->layout = layout;
    gui_widget_request_layout(&panel->base);
    gui_widget_invalidate(&panel->base);
}

void gui_panel_set_style(gui_panel_t *panel, gui_color_t background, gui_color_t border_color, uint8_t border_width)
{
    panel->background = background;
    panel->border_color = border_color;
    panel->border_width = border_width;
    panel->draw_background = true;
    panel->draw_border = border_width > 0;
    gui_widget_invalidate(&panel->base);
}

void gui_panel_set_chrome(gui_panel_t *panel, bool draw_background, bool draw_border)
{
    panel->draw_background = draw_background;
    panel->draw_border = draw_border && panel->border_width > 0;
    gui_widget_invalidate(&panel->base);
}

void gui_panel_set_spacing(gui_panel_t *panel, uint8_t padding, uint8_t gap)
{
    panel->padding = padding;
    panel->gap = gap;
    gui_widget_request_layout(&panel->base);
    gui_widget_invalidate(&panel->base);
}

void gui_screen_init(gui_screen_t *screen)
{
    gui_panel_init(screen);
}
