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

    int16_t cursor = panel->layout == GUI_LAYOUT_ROW ? inner_x : inner_y;
    for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        gui_rect_t frame = child->frame;
        if (panel->layout == GUI_LAYOUT_ROW) {
            const int16_t main_size = frame.width > 0 ? frame.width : 1;
            const int16_t cross_size = frame.height > 0 ? frame.height : inner_h;
            int16_t cross_start = inner_y;
            switch (panel->align) {
                case GUI_LAYOUT_ALIGN_CENTER:
                    cross_start += (int16_t)(((inner_h - cross_size) > 0 ? (inner_h - cross_size) : 0) / 2);
                    break;
                case GUI_LAYOUT_ALIGN_END:
                    cross_start += (inner_h - cross_size) > 0 ? (inner_h - cross_size) : 0;
                    break;
                case GUI_LAYOUT_ALIGN_STRETCH:
                case GUI_LAYOUT_ALIGN_START:
                case GUI_LAYOUT_ALIGN_SPACE_BETWEEN:
                default:
                    break;
            }
            frame.x = cursor;
            frame.y = cross_start;
            frame.width = main_size;
            frame.height = panel->align == GUI_LAYOUT_ALIGN_STRETCH ? inner_h : cross_size;
            cursor += (int16_t)(main_size + panel->gap);
        } else {
            const int16_t main_size = frame.height > 0 ? frame.height : 1;
            const int16_t cross_size = frame.width > 0 ? frame.width : inner_w;
            int16_t cross_start = inner_x;
            switch (panel->align) {
                case GUI_LAYOUT_ALIGN_CENTER:
                    cross_start += (int16_t)(((inner_w - cross_size) > 0 ? (inner_w - cross_size) : 0) / 2);
                    break;
                case GUI_LAYOUT_ALIGN_END:
                    cross_start += (inner_w - cross_size) > 0 ? (inner_w - cross_size) : 0;
                    break;
                case GUI_LAYOUT_ALIGN_STRETCH:
                case GUI_LAYOUT_ALIGN_START:
                case GUI_LAYOUT_ALIGN_SPACE_BETWEEN:
                default:
                    break;
            }
            frame.x = cross_start;
            frame.y = cursor;
            frame.width = panel->align == GUI_LAYOUT_ALIGN_STRETCH ? inner_w : cross_size;
            frame.height = main_size;
            cursor += (int16_t)(main_size + panel->gap);
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

    const gui_renderer_rotation_t rotation = (gui_renderer_rotation_t)(((widget->rotation_degrees % 360) + 360) % 360 / 90);
    const bool rotation_enabled = ((widget->rotation_degrees % 360 + 360) % 360) % 90 == 0 && rotation != GUI_RENDERER_ROTATION_0;
    if (rotation_enabled) {
        gui_renderer_push_rotation(renderer,
                                   rotation,
                                   (gui_point_t){
                                       .x = (int16_t)(rect.x + rect.width / 2),
                                       .y = (int16_t)(rect.y + rect.height / 2),
                                   });
    }

    if (panel->radius > 0) {
        gui_renderer_fill_rounded_rect(renderer,
                                       rect,
                                       panel->radius,
                                       panel->background,
                                       panel->draw_background,
                                       panel->border_color,
                                       panel->border_width,
                                       panel->draw_border);
    } else {
        if (panel->draw_background) {
            gui_renderer_fill_rect(renderer, rect, panel->background);
        }
        if (panel->draw_border && panel->border_width > 0) {
            gui_renderer_stroke_rect(renderer, rect, panel->border_width, panel->border_color);
        }
    }

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
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
    panel->align = GUI_LAYOUT_ALIGN_START;
    panel->justify = GUI_LAYOUT_ALIGN_START;
}

void gui_panel_set_layout(gui_panel_t *panel, gui_layout_t layout)
{
    panel->layout = layout;
    gui_widget_request_layout(&panel->base);
    gui_widget_invalidate(&panel->base);
}

void gui_panel_set_align(gui_panel_t *panel, gui_layout_align_t align, gui_layout_align_t justify)
{
    panel->align = align;
    panel->justify = justify;
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

void gui_panel_set_radius(gui_panel_t *panel, uint8_t radius)
{
    panel->radius = radius;
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
