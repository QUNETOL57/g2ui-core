#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gui/core/gui_types.h"

struct gui_context_t;
struct gui_renderer_t;
struct gui_widget_t;

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_PRESS,
    GUI_EVENT_RELEASE,
    GUI_EVENT_MOVE,
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    gui_point_t position;
} gui_input_event_t;

typedef struct gui_widget_vtable_t {
    gui_size_t (*measure)(struct gui_widget_t *widget, gui_size_t available);
    void (*layout)(struct gui_widget_t *widget);
    void (*paint)(struct gui_widget_t *widget, struct gui_renderer_t *renderer, gui_rect_t clip_rect);
    bool (*input)(struct gui_widget_t *widget, const gui_input_event_t *event);
} gui_widget_vtable_t;

typedef struct gui_widget_t {
    const gui_widget_vtable_t *vtable;
    struct gui_context_t *context;
    struct gui_widget_t *parent;
    struct gui_widget_t *first_child;
    struct gui_widget_t *last_child;
    struct gui_widget_t *next_sibling;
    gui_rect_t frame;
    gui_size_t measured_size;
    int16_t rotation_degrees;
    bool visible;
    bool enabled;
    bool needs_layout;
} gui_widget_t;

void gui_widget_init(gui_widget_t *widget, const gui_widget_vtable_t *vtable);
void gui_widget_add_child(gui_widget_t *parent, gui_widget_t *child);
void gui_widget_set_frame(gui_widget_t *widget, gui_rect_t frame);
void gui_widget_set_visible(gui_widget_t *widget, bool visible);
void gui_widget_set_rotation(gui_widget_t *widget, int16_t rotation_degrees);
void gui_widget_request_layout(gui_widget_t *widget);
void gui_widget_invalidate(gui_widget_t *widget);
gui_rect_t gui_widget_absolute_rect(const gui_widget_t *widget);
void gui_widget_attach_context(gui_widget_t *widget, struct gui_context_t *context);
void gui_widget_layout_tree(gui_widget_t *widget);
void gui_widget_paint_tree(gui_widget_t *widget, struct gui_renderer_t *renderer, gui_rect_t clip_rect);
bool gui_widget_dispatch_input(gui_widget_t *widget, const gui_input_event_t *event);
