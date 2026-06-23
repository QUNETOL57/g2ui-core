#include "gui/widgets/gui_button.h"

#include "gui/render/gui_renderer.h"

static int aligned_offset(gui_button_align_t align, int outer_width, int inner_width)
{
    if (align == GUI_BUTTON_ALIGN_CENTER) {
        return (outer_width - inner_width) / 2;
    }
    if (align == GUI_BUTTON_ALIGN_RIGHT) {
        return outer_width - inner_width;
    }
    return 0;
}

static int vertical_offset(gui_button_vertical_align_t align, int outer_height, int inner_height)
{
    if (align == GUI_BUTTON_VERTICAL_ALIGN_CENTER) {
        return (outer_height - inner_height) / 2;
    }
    if (align == GUI_BUTTON_VERTICAL_ALIGN_BOTTOM) {
        return outer_height - inner_height;
    }
    return 0;
}

static gui_size_t gui_button_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_button_t *button = (gui_button_t *)widget;
    const int text_scale = button->text_scale > 0 ? button->text_scale : 1;
    const int text_width = gui_font_measure_text_width_scaled(button->font, button->text, text_scale);
    const int text_height = (button->font != NULL ? button->font->line_height : 0) * text_scale;
    const int icon_width = button->icon != NULL ? button->icon->width : 0;
    const int icon_height = button->icon != NULL ? button->icon->height : 0;
    const bool vertical_icon = button->icon_position == GUI_BUTTON_ICON_POSITION_TOP ||
                               button->icon_position == GUI_BUTTON_ICON_POSITION_BOTTOM;
    const int icon_gap = (button->icon != NULL && text_width > 0) ? button->icon_gap : 0;
    const int group_width = vertical_icon ? (icon_width > text_width ? icon_width : text_width) : icon_width + icon_gap + text_width;
    const int group_height = vertical_icon ? icon_height + icon_gap + text_height : (icon_height > text_height ? icon_height : text_height);
    gui_size_t size = {
        .width = (int16_t)(group_width + button->padding_left + button->padding_right + button->border_width * 2),
        .height = (int16_t)(group_height + button->padding_top + button->padding_bottom + button->border_width * 2),
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

    const gui_color_t background = button->pressed ? button->pressed_background : button->background;
    if (button->radius > 0) {
        gui_renderer_fill_rounded_rect(renderer,
                                       absolute,
                                       button->radius,
                                       background,
                                       button->draw_background,
                                       button->border_color,
                                       button->border_width,
                                       button->draw_border);
    } else {
        if (button->draw_background) {
            gui_renderer_fill_rect(renderer, absolute, background);
        }
        if (button->draw_border && button->border_width > 0) {
            gui_renderer_stroke_rect(renderer, absolute, button->border_width, button->border_color);
        }
    }

    if (button->font != NULL && button->text != NULL) {
        const int text_scale = button->text_scale > 0 ? button->text_scale : 1;
        const int text_width = gui_font_measure_text_width_scaled(button->font, button->text, text_scale);
        const int text_height = button->font->line_height * text_scale;
        const int icon_width = button->icon != NULL ? button->icon->width : 0;
        const int icon_height = button->icon != NULL ? button->icon->height : 0;
        const int border_width = button->border_width;
        const int content_width = absolute.width - border_width * 2 - button->padding_left - button->padding_right;
        const int content_height = absolute.height - border_width * 2 - button->padding_top - button->padding_bottom;
        const bool icon_vertical = button->icon_position == GUI_BUTTON_ICON_POSITION_TOP ||
                                   button->icon_position == GUI_BUTTON_ICON_POSITION_BOTTOM;
        const int icon_gap = (button->icon != NULL && text_width > 0) ? button->icon_gap : 0;
        const int group_width = icon_vertical
            ? (icon_width > text_width ? icon_width : text_width)
            : icon_width + (button->icon != NULL ? icon_gap : 0) + text_width;
        const int group_height = icon_vertical
            ? icon_height + (button->icon != NULL ? icon_gap : 0) + text_height
            : (icon_height > text_height ? icon_height : text_height);
        const int group_left = aligned_offset((gui_button_align_t)button->horizontal_align, content_width, group_width);
        const int group_top = vertical_offset((gui_button_vertical_align_t)button->vertical_align, content_height, group_height);
        const int icon_left = icon_vertical
            ? group_left + ((group_width - icon_width) > 0 ? (group_width - icon_width) / 2 : 0)
            : group_left + (button->icon_position == GUI_BUTTON_ICON_POSITION_RIGHT ? text_width + icon_gap : 0);
        const int icon_top = icon_vertical
            ? group_top + (button->icon_position == GUI_BUTTON_ICON_POSITION_BOTTOM ? text_height + icon_gap : 0)
            : group_top + ((group_height - icon_height) > 0 ? (group_height - icon_height) / 2 : 0);
        const int text_left = icon_vertical
            ? group_left + ((group_width - text_width) > 0 ? (group_width - text_width) / 2 : 0)
            : group_left + (button->icon_position == GUI_BUTTON_ICON_POSITION_LEFT && button->icon != NULL ? icon_width + icon_gap : 0);
        const int text_top = icon_vertical
            ? group_top + (button->icon_position == GUI_BUTTON_ICON_POSITION_TOP && button->icon != NULL ? icon_height + icon_gap : 0)
            : group_top + ((group_height - text_height) > 0 ? (group_height - text_height) / 2 : 0);
        const int text_box_width = (icon_vertical || button->icon_position == GUI_BUTTON_ICON_POSITION_LEFT)
            ? ((content_width - text_left) > 0 ? (content_width - text_left) : 0)
            : ((icon_left - icon_gap - text_left) > 0 ? (icon_left - icon_gap - text_left) : 0);
        const int text_box_height = (!icon_vertical || button->icon_position == GUI_BUTTON_ICON_POSITION_TOP)
            ? ((content_height - text_top) > 0 ? (content_height - text_top) : 0)
            : ((icon_top - icon_gap - text_top) > 0 ? (icon_top - icon_gap - text_top) : 0);
        const int content_x = absolute.x + border_width + button->padding_left;
        const int content_y = absolute.y + border_width + button->padding_top;

        if (button->icon != NULL) {
            gui_renderer_draw_icon(
                renderer,
                (gui_point_t){
                    .x = (int16_t)(content_x + icon_left),
                    .y = (int16_t)(content_y + icon_top),
                },
                button->icon,
                button->text_color);
        }

        int text_origin_x = content_x + text_left;
        int text_origin_y = content_y + text_top;
        if (button->icon == NULL) {
            text_origin_x = content_x + aligned_offset((gui_button_align_t)button->horizontal_align, content_width, text_width);
            text_origin_y = content_y + vertical_offset((gui_button_vertical_align_t)button->vertical_align, content_height, text_height);
        } else {
            (void)text_box_width;
            (void)text_box_height;
        }
        gui_renderer_draw_text_scaled(
            renderer,
            (gui_point_t){
                .x = (int16_t)text_origin_x,
                .y = (int16_t)text_origin_y,
            },
            button->font,
            button->text,
            button->text_color,
            text_scale);
    }

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
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
    button->padding_top = 6;
    button->padding_right = 10;
    button->padding_bottom = 6;
    button->padding_left = 10;
    button->icon_position = GUI_BUTTON_ICON_POSITION_LEFT;
    button->icon_gap = 2;
    button->horizontal_align = GUI_BUTTON_ALIGN_CENTER;
    button->vertical_align = GUI_BUTTON_VERTICAL_ALIGN_CENTER;
    button->draw_background = true;
    button->draw_border = false;
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
    button->draw_background = true;
    button->draw_border = border_width > 0;
    gui_widget_invalidate(&button->base);
}

void gui_button_set_padding(gui_button_t *button, uint8_t padding_x, uint8_t padding_y)
{
    button->padding_x = padding_x;
    button->padding_y = padding_y;
    button->padding_top = padding_y;
    button->padding_right = padding_x;
    button->padding_bottom = padding_y;
    button->padding_left = padding_x;
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_padding_sides(gui_button_t *button, uint8_t top, uint8_t right, uint8_t bottom, uint8_t left)
{
    button->padding_top = top;
    button->padding_right = right;
    button->padding_bottom = bottom;
    button->padding_left = left;
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_radius(gui_button_t *button, uint8_t radius)
{
    button->radius = radius;
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

void gui_button_set_chrome(gui_button_t *button, bool draw_background, bool draw_border)
{
    button->draw_background = draw_background;
    button->draw_border = draw_border && button->border_width > 0;
    gui_widget_invalidate(&button->base);
}

void gui_button_set_icon(gui_button_t *button, const gui_icon_asset_t *icon)
{
    button->icon = icon;
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_icon_layout(gui_button_t *button, gui_button_icon_position_t position, uint8_t icon_gap)
{
    button->icon_position = position;
    button->icon_gap = icon_gap;
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_content_align(gui_button_t *button,
                                  gui_button_align_t horizontal_align,
                                  gui_button_vertical_align_t vertical_align)
{
    button->horizontal_align = (uint8_t)horizontal_align;
    button->vertical_align = (uint8_t)vertical_align;
    gui_widget_invalidate(&button->base);
}

void gui_button_set_on_click(gui_button_t *button, gui_button_click_fn on_click, void *user_data)
{
    button->on_click = on_click;
    button->user_data = user_data;
}
