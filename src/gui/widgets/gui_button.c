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

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static void apply_gap_padding(gui_button_icon_slot_t *slot, gui_button_icon_position_t position, uint8_t gap)
{
    slot->padding_top = 0;
    slot->padding_right = 0;
    slot->padding_bottom = 0;
    slot->padding_left = 0;
    if (position == GUI_BUTTON_ICON_POSITION_LEFT) {
        slot->padding_right = gap;
    } else if (position == GUI_BUTTON_ICON_POSITION_RIGHT) {
        slot->padding_left = gap;
    } else if (position == GUI_BUTTON_ICON_POSITION_TOP) {
        slot->padding_bottom = gap;
    } else {
        slot->padding_top = gap;
    }
}

typedef struct {
    const gui_button_icon_slot_t *slot;
    int pad_l;
    int pad_r;
    int pad_t;
    int pad_b;
    int iw;
    int ih;
    int outer_w;
    int outer_h;
} button_icon_metrics_t;

typedef struct {
    button_icon_metrics_t *items[GUI_BUTTON_MAX_ICONS];
    uint8_t count;
} button_icon_side_t;

static void collect_icon_metrics(const gui_button_t *button,
                                 button_icon_metrics_t metrics[GUI_BUTTON_MAX_ICONS],
                                 button_icon_side_t sides[4])
{
    sides[0].count = sides[1].count = sides[2].count = sides[3].count = 0;
    for (uint8_t i = 0; i < button->icon_count && i < GUI_BUTTON_MAX_ICONS; i++) {
        const gui_button_icon_slot_t *slot = &button->icons[i];
        if (slot->icon == NULL) {
            continue;
        }
        uint8_t side = slot->position;
        if (side > GUI_BUTTON_ICON_POSITION_BOTTOM) {
            side = GUI_BUTTON_ICON_POSITION_LEFT;
        }
        button_icon_metrics_t *m = &metrics[i];
        m->slot = slot;
        m->pad_l = slot->padding_left;
        m->pad_r = slot->padding_right;
        m->pad_t = slot->padding_top;
        m->pad_b = slot->padding_bottom;
        m->iw = slot->icon->width;
        m->ih = slot->icon->height;
        m->outer_w = m->pad_l + m->iw + m->pad_r;
        m->outer_h = m->pad_t + m->ih + m->pad_b;
        sides[side].items[sides[side].count++] = m;
    }
}

static int sum_outer(const button_icon_side_t *side, bool width)
{
    int sum = 0;
    for (uint8_t i = 0; i < side->count; i++) {
        sum += width ? side->items[i]->outer_w : side->items[i]->outer_h;
    }
    return sum;
}

static int max_outer(const button_icon_side_t *side, bool width)
{
    int max = 0;
    for (uint8_t i = 0; i < side->count; i++) {
        const int value = width ? side->items[i]->outer_w : side->items[i]->outer_h;
        if (value > max) {
            max = value;
        }
    }
    return max;
}

static void button_content_metrics(const gui_button_t *button,
                                   int *text_width,
                                   int *text_height,
                                   int *group_width,
                                   int *group_height)
{
    const int text_scale = button->text_scale > 0 ? button->text_scale : 1;
    const bool has_text = button->font != NULL && button->text != NULL && button->text[0] != '\0';
    *text_width = has_text ? gui_font_measure_text_width_scaled(button->font, button->text, text_scale) : 0;
    *text_height = has_text ? button->font->line_height * text_scale : 0;

    button_icon_metrics_t metrics[GUI_BUTTON_MAX_ICONS];
    button_icon_side_t sides[4];
    collect_icon_metrics(button, metrics, sides);

    const int left_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_LEFT], true);
    const int right_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_RIGHT], true);
    const int top_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_TOP], true);
    const int bottom_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_BOTTOM], true);
    const int left_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_LEFT], false);
    const int right_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_RIGHT], false);
    const int top_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_TOP], false);
    const int bottom_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_BOTTOM], false);
    const int mid_w = left_w + *text_width + right_w;
    const int mid_h = max_int(max_int(left_h, *text_height), right_h);
    *group_width = max_int(max_int(max_int(top_w, mid_w), bottom_w), *text_width);
    *group_height = top_h + max_int(mid_h, *text_height) + bottom_h;
}

static gui_size_t gui_button_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_button_t *button = (gui_button_t *)widget;
    int text_width = 0;
    int text_height = 0;
    int group_width = 0;
    int group_height = 0;
    button_content_metrics(button, &text_width, &text_height, &group_width, &group_height);
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

static void place_icon_row(const button_icon_side_t *side,
                           int start_x,
                           int row_top,
                           int row_height,
                           gui_renderer_t *renderer,
                           int origin_x,
                           int origin_y,
                           gui_color_t fallback_color)
{
    int x = start_x;
    for (uint8_t i = 0; i < side->count; i++) {
        const button_icon_metrics_t *m = side->items[i];
        const int icon_left = x + m->pad_l;
        const int icon_top = row_top + max_int(0, (row_height - m->outer_h) / 2) + m->pad_t;
        const gui_color_t color = m->slot->has_color ? m->slot->color : fallback_color;
        gui_renderer_draw_icon(
            renderer,
            (gui_point_t){
                .x = (int16_t)(origin_x + icon_left),
                .y = (int16_t)(origin_y + icon_top),
            },
            m->slot->icon,
            color);
        x += m->outer_w;
    }
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

    const int text_scale = button->text_scale > 0 ? button->text_scale : 1;
    const bool has_text = button->font != NULL && button->text != NULL && button->text[0] != '\0';
    const int text_width = has_text ? gui_font_measure_text_width_scaled(button->font, button->text, text_scale) : 0;
    const int text_height = has_text ? button->font->line_height * text_scale : 0;
    const int border_width = button->border_width;
    const int content_width = absolute.width - border_width * 2 - button->padding_left - button->padding_right;
    const int content_height = absolute.height - border_width * 2 - button->padding_top - button->padding_bottom;
    const int content_x = absolute.x + border_width + button->padding_left;
    const int content_y = absolute.y + border_width + button->padding_top;

    button_icon_metrics_t metrics[GUI_BUTTON_MAX_ICONS];
    button_icon_side_t sides[4];
    collect_icon_metrics(button, metrics, sides);

    const int left_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_LEFT], true);
    const int right_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_RIGHT], true);
    const int top_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_TOP], true);
    const int bottom_w = sum_outer(&sides[GUI_BUTTON_ICON_POSITION_BOTTOM], true);
    const int left_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_LEFT], false);
    const int right_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_RIGHT], false);
    const int top_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_TOP], false);
    const int bottom_h = max_outer(&sides[GUI_BUTTON_ICON_POSITION_BOTTOM], false);
    const int mid_w = left_w + text_width + right_w;
    const int mid_h = max_int(max_int(left_h, text_height), right_h);
    const int group_width = max_int(max_int(max_int(top_w, mid_w), bottom_w), text_width);
    const int group_height = top_h + max_int(mid_h, text_height) + bottom_h;
    const int group_left = max_int(0, aligned_offset((gui_button_align_t)button->horizontal_align, content_width, group_width));
    const int group_top = max_int(0, vertical_offset((gui_button_vertical_align_t)button->vertical_align, content_height, group_height));
    const int mid_top = group_top + top_h;
    const int mid_height = max_int(mid_h, text_height);
    const int mid_left = group_left + max_int(0, (group_width - mid_w) / 2);
    const int text_left = mid_left + left_w;
    const int text_top = mid_top + max_int(0, (mid_height - text_height) / 2);
    const int bottom_top = mid_top + mid_height;

    place_icon_row(&sides[GUI_BUTTON_ICON_POSITION_TOP],
                   group_left + max_int(0, (group_width - top_w) / 2),
                   group_top,
                   top_h,
                   renderer,
                   content_x,
                   content_y,
                   button->text_color);
    place_icon_row(&sides[GUI_BUTTON_ICON_POSITION_LEFT],
                   mid_left,
                   mid_top,
                   mid_height,
                   renderer,
                   content_x,
                   content_y,
                   button->text_color);
    place_icon_row(&sides[GUI_BUTTON_ICON_POSITION_RIGHT],
                   text_left + text_width,
                   mid_top,
                   mid_height,
                   renderer,
                   content_x,
                   content_y,
                   button->text_color);
    place_icon_row(&sides[GUI_BUTTON_ICON_POSITION_BOTTOM],
                   group_left + max_int(0, (group_width - bottom_w) / 2),
                   bottom_top,
                   bottom_h,
                   renderer,
                   content_x,
                   content_y,
                   button->text_color);

    if (has_text) {
        gui_renderer_draw_text_scaled(
            renderer,
            (gui_point_t){
                .x = (int16_t)(content_x + text_left),
                .y = (int16_t)(content_y + text_top),
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
    button->icon = NULL;
    button->icon_count = 0;
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
    if (icon == NULL) {
        button->icon_count = 0;
    } else {
        gui_button_icon_slot_t slot = { 0 };
        slot.icon = icon;
        slot.position = button->icon_position;
        apply_gap_padding(&slot, (gui_button_icon_position_t)button->icon_position, button->icon_gap);
        button->icons[0] = slot;
        button->icon_count = 1;
    }
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_icon_layout(gui_button_t *button, gui_button_icon_position_t position, uint8_t icon_gap)
{
    button->icon_position = position;
    button->icon_gap = icon_gap;
    if (button->icon_count == 1) {
        button->icons[0].position = (uint8_t)position;
        apply_gap_padding(&button->icons[0], position, icon_gap);
    }
    gui_widget_request_layout(&button->base);
    gui_widget_invalidate(&button->base);
}

void gui_button_set_icons(gui_button_t *button, const gui_button_icon_slot_t *slots, uint8_t count)
{
    button->icon_count = 0;
    button->icon = NULL;
    if (slots == NULL) {
        gui_widget_request_layout(&button->base);
        gui_widget_invalidate(&button->base);
        return;
    }
    const uint8_t limit = count > GUI_BUTTON_MAX_ICONS ? GUI_BUTTON_MAX_ICONS : count;
    for (uint8_t i = 0; i < limit; i++) {
        if (slots[i].icon == NULL) {
            continue;
        }
        button->icons[button->icon_count] = slots[i];
        if (button->icons[button->icon_count].position > GUI_BUTTON_ICON_POSITION_BOTTOM) {
            button->icons[button->icon_count].position = GUI_BUTTON_ICON_POSITION_LEFT;
        }
        button->icon_count++;
    }
    if (button->icon_count > 0) {
        button->icon = button->icons[0].icon;
        button->icon_position = button->icons[0].position;
    }
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
