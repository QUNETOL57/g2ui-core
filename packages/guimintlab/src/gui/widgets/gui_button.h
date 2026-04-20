#pragma once

#include "gui/core/gui_assets.h"
#include "gui/core/gui_widget.h"

typedef void (*gui_button_click_fn)(void *user_data);

typedef struct {
    gui_widget_t base;
    const gui_font_t *font;
    const char *text;
    gui_color_t text_color;
    gui_color_t background;
    gui_color_t pressed_background;
    gui_color_t border_color;
    uint8_t border_width;
    uint8_t padding_x;
    uint8_t padding_y;
    uint8_t text_scale;
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
void gui_button_set_text_scale(gui_button_t *button, uint8_t text_scale);
void gui_button_set_on_click(gui_button_t *button, gui_button_click_fn on_click, void *user_data);
