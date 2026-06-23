#pragma once

#include "gui/core/gui_assets.h"
#include "gui/core/gui_widget.h"

typedef void (*gui_button_click_fn)(void *user_data);

typedef enum {
    GUI_BUTTON_ICON_POSITION_LEFT = 0,
    GUI_BUTTON_ICON_POSITION_RIGHT,
    GUI_BUTTON_ICON_POSITION_TOP,
    GUI_BUTTON_ICON_POSITION_BOTTOM,
} gui_button_icon_position_t;

typedef enum {
    GUI_BUTTON_ALIGN_LEFT = 0,
    GUI_BUTTON_ALIGN_CENTER,
    GUI_BUTTON_ALIGN_RIGHT,
} gui_button_align_t;

typedef enum {
    GUI_BUTTON_VERTICAL_ALIGN_TOP = 0,
    GUI_BUTTON_VERTICAL_ALIGN_CENTER,
    GUI_BUTTON_VERTICAL_ALIGN_BOTTOM,
} gui_button_vertical_align_t;

typedef struct {
    gui_widget_t base;
    const gui_font_t *font;
    const char *text;
    const gui_icon_asset_t *icon;
    gui_color_t text_color;
    gui_color_t background;
    gui_color_t pressed_background;
    gui_color_t border_color;
    uint8_t border_width;
    uint8_t radius;
    uint8_t padding_x;
    uint8_t padding_y;
    uint8_t padding_top;
    uint8_t padding_right;
    uint8_t padding_bottom;
    uint8_t padding_left;
    uint8_t icon_gap;
    uint8_t icon_position;
    uint8_t horizontal_align;
    uint8_t vertical_align;
    uint8_t text_scale;
    bool draw_background;
    bool draw_border;
    bool pressed;
    gui_button_click_fn on_click;
    void *user_data;
} gui_button_t;

void gui_button_init(gui_button_t *button, const gui_font_t *font, const char *text);
void gui_button_set_style(
    gui_button_t *button,
    gui_color_t text_color,
    gui_color_t background,
    gui_color_t pressed_background,
    gui_color_t border_color,
    uint8_t border_width);
void gui_button_set_padding(gui_button_t *button, uint8_t padding_x, uint8_t padding_y);
void gui_button_set_padding_sides(gui_button_t *button, uint8_t top, uint8_t right, uint8_t bottom, uint8_t left);
void gui_button_set_radius(gui_button_t *button, uint8_t radius);
void gui_button_set_text_scale(gui_button_t *button, uint8_t text_scale);
void gui_button_set_chrome(gui_button_t *button, bool draw_background, bool draw_border);
void gui_button_set_icon(gui_button_t *button, const gui_icon_asset_t *icon);
void gui_button_set_icon_layout(gui_button_t *button, gui_button_icon_position_t position, uint8_t icon_gap);
void gui_button_set_content_align(gui_button_t *button,
                                  gui_button_align_t horizontal_align,
                                  gui_button_vertical_align_t vertical_align);
void gui_button_set_on_click(gui_button_t *button, gui_button_click_fn on_click, void *user_data);
