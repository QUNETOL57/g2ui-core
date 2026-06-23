#include "gui/core/gui_widget.h"

#include <string.h>

#include "gui/core/gui_context.h"
#include "gui/render/gui_renderer.h"

void gui_widget_init(gui_widget_t *widget, const gui_widget_vtable_t *vtable)
{
    memset(widget, 0, sizeof(*widget));
    widget->vtable = vtable;
    widget->visible = true;
    widget->enabled = true;
    widget->needs_layout = true;
}

void gui_widget_attach_context(gui_widget_t *widget, struct gui_context_t *context)
{
    if (widget == NULL) {
        return;
    }

    widget->context = context;
    for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        gui_widget_attach_context(child, context);
    }
}

void gui_widget_add_child(gui_widget_t *parent, gui_widget_t *child)
{
    if (parent == NULL || child == NULL) {
        return;
    }

    child->parent = parent;
    child->next_sibling = NULL;
    if (parent->last_child != NULL) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
    gui_widget_attach_context(child, parent->context);
    gui_widget_request_layout(parent);
    gui_widget_invalidate(child);
}

gui_rect_t gui_widget_absolute_rect(const gui_widget_t *widget)
{
    gui_rect_t rect = widget->frame;
    for (const gui_widget_t *node = widget->parent; node != NULL; node = node->parent) {
        rect = gui_rect_translate(rect, node->frame.x, node->frame.y);
    }
    return rect;
}

void gui_widget_set_frame(gui_widget_t *widget, gui_rect_t frame)
{
    if (widget == NULL) {
        return;
    }

    gui_widget_invalidate(widget);
    widget->frame = frame;
    widget->needs_layout = true;
    gui_widget_invalidate(widget);
}

void gui_widget_set_visible(gui_widget_t *widget, bool visible)
{
    if (widget == NULL || widget->visible == visible) {
        return;
    }

    widget->visible = visible;
    gui_widget_invalidate(widget);
}

void gui_widget_set_rotation(gui_widget_t *widget, int16_t rotation_degrees)
{
    if (widget == NULL) {
        return;
    }
    if (widget->rotation_degrees == rotation_degrees) {
        return;
    }
    widget->rotation_degrees = rotation_degrees;
    gui_widget_invalidate(widget);
}

void gui_widget_request_layout(gui_widget_t *widget)
{
    if (widget == NULL) {
        return;
    }

    widget->needs_layout = true;
    if (widget->parent != NULL) {
        gui_widget_request_layout(widget->parent);
    }
}

void gui_widget_invalidate(gui_widget_t *widget)
{
    if (widget == NULL || widget->context == NULL) {
        return;
    }

    gui_context_invalidate(widget->context, gui_widget_absolute_rect(widget));
}

static gui_size_t gui_widget_default_measure(struct gui_widget_t *widget, gui_size_t available)
{
    (void)widget;
    return available;
}

void gui_widget_layout_tree(gui_widget_t *widget)
{
    if (widget == NULL || !widget->visible) {
        return;
    }

    const gui_size_t available = {
        .width = widget->frame.width,
        .height = widget->frame.height,
    };
    widget->measured_size = widget->vtable != NULL && widget->vtable->measure != NULL
        ? widget->vtable->measure(widget, available)
        : gui_widget_default_measure(widget, available);

    if (widget->needs_layout && widget->vtable != NULL && widget->vtable->layout != NULL) {
        widget->vtable->layout(widget);
        widget->needs_layout = false;
    }

    for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        gui_widget_layout_tree(child);
    }
}

void gui_widget_paint_tree(gui_widget_t *widget, struct gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    if (widget == NULL || renderer == NULL || !widget->visible) {
        return;
    }

    const gui_rect_t absolute_rect = gui_widget_absolute_rect(widget);
    const gui_rect_t clipped = gui_rect_intersect(absolute_rect, clip_rect);
    if (gui_rect_is_empty(clipped)) {
        return;
    }

    if (widget->vtable != NULL && widget->vtable->paint != NULL) {
        widget->vtable->paint(widget, renderer, clipped);
    }

    for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        gui_widget_paint_tree(child, renderer, clipped);
    }
}

bool gui_widget_dispatch_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    if (widget == NULL || event == NULL || !widget->visible || !widget->enabled) {
        return false;
    }

    for (gui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        if (gui_widget_dispatch_input(child, event)) {
            return true;
        }
    }

    if (!gui_rect_contains_point(gui_widget_absolute_rect(widget), event->position)) {
        return false;
    }

    return widget->vtable != NULL &&
           widget->vtable->input != NULL &&
           widget->vtable->input(widget, event);
}
