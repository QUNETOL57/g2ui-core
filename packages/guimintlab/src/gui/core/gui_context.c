#include "gui/core/gui_context.h"

#include <string.h>

static gui_rect_t gui_context_normalize_dirty(gui_context_t *context, gui_rect_t rect)
{
    return gui_rect_intersect(rect, gui_display_bounds(context->display));
}

esp_err_t gui_context_init(gui_context_t *context, gui_display_t *display)
{
    if (context == NULL || display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(context, 0, sizeof(*context));
    context->display = display;
    return gui_renderer_init(&context->renderer, display);
}

void gui_context_set_root(gui_context_t *context, gui_widget_t *root)
{
    if (context == NULL) {
        return;
    }

    context->root = root;
    if (root != NULL) {
        gui_widget_attach_context(root, context);
        gui_widget_set_frame(root, gui_display_bounds(context->display));
    }
    context->needs_full_redraw = true;
}

void gui_context_invalidate(gui_context_t *context, gui_rect_t rect)
{
    if (context == NULL) {
        return;
    }

    rect = gui_context_normalize_dirty(context, rect);
    if (gui_rect_is_empty(rect)) {
        return;
    }

    for (uint8_t i = 0; i < context->dirty_count; i++) {
        if (gui_rect_overlaps(context->dirty_rects[i], rect)) {
            context->dirty_rects[i] = gui_rect_union(context->dirty_rects[i], rect);
            return;
        }
    }

    if (context->dirty_count < GUI_DIRTY_RECTS_MAX) {
        context->dirty_rects[context->dirty_count++] = rect;
        return;
    }

    context->needs_full_redraw = true;
}

void gui_context_render(gui_context_t *context)
{
    if (context == NULL || context->root == NULL) {
        return;
    }

    const gui_rect_t full_bounds = gui_display_bounds(context->display);
    gui_widget_layout_tree(context->root);

    if (context->needs_full_redraw) {
        context->dirty_count = 1;
        context->dirty_rects[0] = full_bounds;
        context->needs_full_redraw = false;
    }

    for (uint8_t i = 0; i < context->dirty_count; i++) {
        const gui_rect_t dirty = context->dirty_rects[i];
        if (gui_rect_is_empty(dirty)) {
            continue;
        }

        gui_renderer_set_clip(&context->renderer, dirty);
        gui_widget_paint_tree(context->root, &context->renderer, dirty);
        gui_renderer_present(&context->renderer, dirty);
    }

    context->dirty_count = 0;
    gui_renderer_set_clip(&context->renderer, full_bounds);
}

bool gui_context_dispatch_input(gui_context_t *context, const gui_input_event_t *event)
{
    if (context == NULL || context->root == NULL || event == NULL) {
        return false;
    }

    return gui_widget_dispatch_input(context->root, event);
}
