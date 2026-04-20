#pragma once

#include "gui/core/gui_assets.h"
#include "gui/core/gui_widget.h"

typedef struct {
    gui_widget_t base;
    const gui_icon_asset_t *icon;
    gui_color_t color;
} gui_icon_widget_t;

void gui_icon_widget_init(gui_icon_widget_t *icon_widget, const gui_icon_asset_t *icon, gui_color_t color);
void gui_icon_widget_set_icon(gui_icon_widget_t *icon_widget, const gui_icon_asset_t *icon);
