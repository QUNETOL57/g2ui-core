#include "gui/widgets/gui_button.h"

#include "gui/render/gui_renderer.h"

static gui_size_t gui_button_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_button_t *button = (gui_button_t *)widget;
    const int text_scale = button->text_scale > 0 ? button->text_scale : 1;
    const int text_width = gui_font_measure_text_width_scaled(button->font, button->text, text_scale);
    gui_size_t size = {
        .width = (int16_t)(text_width + button->padding_x * 2),
        .height = (int16_t)(((button->font != NULL ? button->font->line_height : 0) * text_scale) + button->padding_y * 2),
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

static void gui_button_layout(gui_widget_t *widget)
{
    (void)widget;
}

static void gui_button_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_button_t *button = (gui_button_t *)widget;
    const gui_rect_t absolute = gui_widget_absolute_rect(widget);
    if (gui_rect_is_empty(gui_rect_intersect(absolute, clip_rect))) {
        return;
    }

    gui_renderer_fill_rect(renderer, absolute, button->pressed ? button->pressed_background : button->background);
    if (button->border_width > 0) {
        gui_renderer_stroke_rect(renderer, absolute, button->border_width, button->border_color);
    }

    if (button->text != NULL && button->font != NULL) {
        const int text_scale = button->text_scale > 0 ? button->text_scale : 1;
        const int text_width = gui_font_measure_text_width_scaled(button->font, button->text, text_scale);
        const int text_height = button->font->line_height * text_scale;
        const gui_point_t origin = {
            .x = absolute.x + (absolute.width - text_width) / 2,
            .y = absolute.y + (absolute.height - text_height) / 2,
        };
        gui_renderer_draw_text_scaled(renderer, origin, button->font, button->text, button->text_color, text_scale);
    }
}

static bool gui_button_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    gui_button_t *button = (gui_button_t *)widget;

    switch (event->type) {
        case GUI_EVENT_PRESS:
            button->pressed = true;
            gui_widget_invalidate(widget);
            return true;
        case GUI_EVENT_RELEASE:
            if (button->pressed) {
                button->pressed = false;
                gui_widget_invalidate(widget);
                if (button->on_click != NULL) {
                    button->on_click(button->user_data);
                }
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

static const gui_widget_vtable_t GUI_BUTTON_VTABLE = {
    .measure = gui_button_measure,
    .layout = gui_button_layout,
    .paint = gui_button_paint,
    .input = gui_button_input,
};

void gui_button_init(gui_button_t *button, const gui_font_t *font, const char *text)
{
    gui_widget_init(&button->base, &GUI_BUTTON_VTABLE);
    button->font = font;
    button->text = text;
    button->padding_x = 10;
    button->padding_y = 6;
    button->text_scale = 1;
}

void gui_button_set_style(
    gui_button_t *button,
    gui_color_t text_color,
    gui_color_t background,
    gui_color_t pressed_background,
    gui_color_t border_color,
    uint8_t border_width)
{
    button->text_color = text_color;
    button->background = background;
    button->pressed_background = pressed_background;
    button->border_color = border_color;
    button->border_width = border_width;
    gui_widget_invalidate(&button->base);
}

void gui_button_set_padding(gui_button_t *button, uint8_t padding_x, uint8_t padding_y)
{
    button->padding_x = padding_x;
    button->padding_y = padding_y;
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_text_scale(gui_button_t *button, uint8_t text_scale)
{
    if (text_scale == 0) {
        text_scale = 1;
    }
    button->text_scale = text_scale;
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_on_click(gui_button_t *button, gui_button_click_fn on_click, void *user_data)
{
    button->on_click = on_click;
    button->user_data = user_data;
}
