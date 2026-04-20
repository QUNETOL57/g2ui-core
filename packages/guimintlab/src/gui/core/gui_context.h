#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gui/core/gui_types.h"
#include "gui/core/gui_widget.h"
#include "gui/display/gui_display.h"
#include "gui/render/gui_renderer.h"

#define GUI_DIRTY_RECTS_MAX 8

typedef struct gui_context_t {
    gui_display_t *display;
    gui_renderer_t renderer;
    gui_widget_t *root;
    gui_rect_t dirty_rects[GUI_DIRTY_RECTS_MAX];
    uint8_t dirty_count;
    bool needs_full_redraw;
} gui_context_t;

esp_err_t gui_context_init(gui_context_t *context, gui_display_t *display);
void gui_context_set_root(gui_context_t *context, gui_widget_t *root);
void gui_context_invalidate(gui_context_t *context, gui_rect_t rect);
void gui_context_render(gui_context_t *context);
bool gui_context_dispatch_input(gui_context_t *context, const gui_input_event_t *event);
