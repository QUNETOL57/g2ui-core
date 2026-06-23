#pragma once

#include "gui/core/gui_assets.h"
#include "gui/core/gui_widget.h"

typedef struct {
    gui_widget_t base;
    const gui_font_t *font;
    const char *text;
    gui_color_t color;
    uint8_t scale;
    uint8_t align;
    uint8_t vertical_align;
    bool text_auto_size;
} gui_label_t;

void gui_label_init(gui_label_t *label, const gui_font_t *font, const char *text, gui_color_t color);
void gui_label_set_text(gui_label_t *label, const char *text);
void gui_label_set_scale(gui_label_t *label, uint8_t scale);
void gui_label_set_align(gui_label_t *label, uint8_t align);
void gui_label_set_vertical_align(gui_label_t *label, uint8_t vertical_align);
void gui_label_set_text_auto_size(gui_label_t *label, bool enabled);

enum {
    GUI_LABEL_ALIGN_LEFT = 0,
    GUI_LABEL_ALIGN_CENTER = 1,
    GUI_LABEL_ALIGN_RIGHT = 2,
};

enum {
    GUI_LABEL_VERTICAL_ALIGN_TOP = 0,
    GUI_LABEL_VERTICAL_ALIGN_CENTER = 1,
    GUI_LABEL_VERTICAL_ALIGN_BOTTOM = 2,
};
