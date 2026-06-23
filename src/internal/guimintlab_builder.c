#include "guimintlab_builder.h"

#include <string.h>

#include "gui/widgets/gui_button.h"
#include "gui/widgets/gui_freehand.h"
#include "gui/widgets/gui_icon.h"
#include "gui/widgets/gui_label.h"
#include "gui/widgets/gui_panel.h"
#include "gui/widgets/gui_shape.h"

/*
 * Internal storage.
 *
 * We carve the user-provided arena into:
 *   - node_capacity * sizeof(gml_node_t)   nodes
 *   - id_capacity   * sizeof(gml_id_entry) id entries
 *
 * Each node stores the widget inline so generated code does not juggle
 * per-widget pointers. Unused variants waste space, but widget scope v1 is
 * tiny and firmware usually has a fixed arena budget anyway.
 */

typedef union {
    gui_panel_t panel;
    gui_label_t label;
    gui_button_t button;
    gui_icon_widget_t icon;
    gui_shape_t shape;
    gui_freehand_t freehand;
} gml_widget_storage_t;

typedef struct {
    gml_widget_storage_t storage;
    gml_widget_type_t    type;
    const char          *id;
    gml_handle_t         parent;
    gml_handle_t         screen;
    gml_action_kind_t    on_press_kind;
    const char          *on_press_target;
    const char          *on_press_value;
    const gml_binding_t *bindings;
    uint16_t             binding_count;
} gml_node_t;

typedef struct {
    const char  *id;
    gml_handle_t handle;
} gml_id_entry_t;

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

static void *arena_alloc(gml_project_t *p, size_t size, size_t align) {
    size_t offset = ALIGN_UP(p->arena_used, align);
    if (offset + size > p->arena_size) return NULL;
    void *ptr = p->arena + offset;
    p->arena_used = offset + size;
    memset(ptr, 0, size);
    return ptr;
}

static gml_node_t *nodes(const gml_project_t *p) { return (gml_node_t *)p->nodes; }
static gml_id_entry_t *ids(const gml_project_t *p) { return (gml_id_entry_t *)p->ids; }

static gml_node_t *node_at(const gml_project_t *p, gml_handle_t handle) {
    if (handle == GML_HANDLE_INVALID || handle >= p->node_count) return NULL;
    return &nodes(p)[handle];
}

static gui_widget_t *widget_base(gml_node_t *node) {
    switch (node->type) {
        case GML_WIDGET_TYPE_SCREEN:
        case GML_WIDGET_TYPE_PANEL:
        case GML_WIDGET_TYPE_IMAGE:
            return &node->storage.panel.base;
        case GML_WIDGET_TYPE_RECT:
        case GML_WIDGET_TYPE_LINE:
        case GML_WIDGET_TYPE_CIRCLE:
        case GML_WIDGET_TYPE_TRIANGLE:
            return &node->storage.shape.base;
        case GML_WIDGET_TYPE_FREEHAND:
            return &node->storage.freehand.base;
        case GML_WIDGET_TYPE_LABEL:
            return &node->storage.label.base;
        case GML_WIDGET_TYPE_BUTTON:
            return &node->storage.button.base;
        case GML_WIDGET_TYPE_ICON:
            return &node->storage.icon.base;
        default:
            return NULL;
    }
}

static bool register_id(gml_project_t *p, const char *id, gml_handle_t handle) {
    if (!id) return false;
    gml_id_entry_t *table = ids(p);
    for (uint16_t i = 0; i < p->id_count; ++i) {
        if (strcmp(table[i].id, id) == 0) return false;  /* duplicate */
    }
    if (p->id_count >= p->id_capacity) return false;
    table[p->id_count].id = id;
    table[p->id_count].handle = handle;
    p->id_count++;
    return true;
}

static gml_handle_t lookup_id(const gml_project_t *p, const char *id) {
    if (!id) return GML_HANDLE_INVALID;
    const gml_id_entry_t *table = ids(p);
    for (uint16_t i = 0; i < p->id_count; ++i) {
        if (strcmp(table[i].id, id) == 0) return table[i].handle;
    }
    return GML_HANDLE_INVALID;
}

static const gui_icon_asset_t *find_icon_asset(const gml_project_t *p, const char *icon_id)
{
    const gml_icon_registry_t *reg = p && p->config ? p->config->icons : NULL;
    if (reg == NULL || icon_id == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < reg->count; ++i) {
        if (reg->entries[i].icon_id && strcmp(reg->entries[i].icon_id, icon_id) == 0) {
            return reg->entries[i].asset;
        }
    }
    return NULL;
}

static gml_handle_t allocate_node(gml_project_t *p, gml_widget_type_t type, const char *id) {
    if (p->node_count >= p->node_capacity) return GML_HANDLE_INVALID;
    const gml_handle_t handle = (gml_handle_t)p->node_count;
    gml_node_t *node = &nodes(p)[handle];
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->id = id;
    node->parent = GML_HANDLE_INVALID;
    node->screen = GML_HANDLE_INVALID;
    node->on_press_kind = GML_ACTION_NONE;
    p->node_count++;

    if (id && !register_id(p, id, handle)) {
        /* silently ignore duplicate: keeps builder forgiving for partial rebuilds */
    }
    return handle;
}

static void init_widget_for_type(gml_node_t *node, const gml_theme_t *theme, const gui_font_t *default_font) {
    switch (node->type) {
        case GML_WIDGET_TYPE_SCREEN:
        case GML_WIDGET_TYPE_PANEL:
        case GML_WIDGET_TYPE_IMAGE:
            gui_panel_init(&node->storage.panel);
            /* No implicit chrome: panel-like widgets stay visually empty until
             * JSON style explicitly enables background and/or border. */
            break;
        case GML_WIDGET_TYPE_RECT:
            gui_shape_init(&node->storage.shape, GUI_SHAPE_KIND_RECT);
            break;
        case GML_WIDGET_TYPE_LINE:
            gui_shape_init(&node->storage.shape, GUI_SHAPE_KIND_LINE);
            break;
        case GML_WIDGET_TYPE_CIRCLE:
            gui_shape_init(&node->storage.shape, GUI_SHAPE_KIND_CIRCLE);
            break;
        case GML_WIDGET_TYPE_TRIANGLE:
            gui_shape_init(&node->storage.shape, GUI_SHAPE_KIND_TRIANGLE);
            break;
        case GML_WIDGET_TYPE_FREEHAND:
            gui_freehand_init(&node->storage.freehand);
            break;
        case GML_WIDGET_TYPE_LABEL: {
            const gui_font_t *font = default_font;
            if (!font && theme) font = theme->font_body;
            gui_label_init(&node->storage.label, font, "", theme ? theme->text_primary : 0xFFFFu);
            break;
        }
        case GML_WIDGET_TYPE_BUTTON: {
            const gui_font_t *font = default_font;
            if (!font && theme) font = theme->font_body;
            gui_button_init(&node->storage.button, font, "");
            if (theme) {
                gui_button_set_style(&node->storage.button,
                                     theme->text_primary,
                                     theme->surface_raised,
                                     theme->accent_soft,
                                     theme->line_strong,
                                     theme->border_thin);
            }
            break;
        }
        case GML_WIDGET_TYPE_ICON:
            gui_icon_widget_init(&node->storage.icon, NULL, theme ? theme->text_primary : 0xFFFFu);
            break;
        default:
            break;
    }
}

static void attach_to_parent(gml_project_t *p, gml_node_t *node, gml_handle_t node_handle, gml_handle_t parent) {
    (void)node_handle;
    if (parent == GML_HANDLE_INVALID) {
        if (p->current_screen != GML_HANDLE_INVALID) {
            parent = p->current_screen;
        } else {
            return;  /* detached */
        }
    }
    gml_node_t *parent_node = node_at(p, parent);
    gui_widget_t *parent_widget = parent_node ? widget_base(parent_node) : NULL;
    gui_widget_t *child_widget = widget_base(node);
    if (parent_widget && child_widget) {
        gui_widget_add_child(parent_widget, child_widget);
    }
    node->parent = parent;
    node->screen = p->current_screen;
}

/* -------------------------------------------------------------------------- */

bool gml_project_init(gml_project_t *project, const gml_project_config_t *config) {
    if (!project || !config || !config->arena || config->arena_size == 0) return false;
    memset(project, 0, sizeof(*project));
    project->config      = config;
    project->arena       = config->arena;
    project->arena_size  = config->arena_size;
    project->arena_used  = 0;
    project->current_screen = GML_HANDLE_INVALID;
    project->active_screen  = GML_HANDLE_INVALID;

    uint16_t node_cap = config->max_nodes ? config->max_nodes : 48;
    uint16_t id_cap   = config->max_ids   ? config->max_ids   : node_cap;

    project->nodes = arena_alloc(project, (size_t)node_cap * sizeof(gml_node_t), _Alignof(gml_node_t));
    project->ids   = arena_alloc(project, (size_t)id_cap   * sizeof(gml_id_entry_t), _Alignof(gml_id_entry_t));
    if (!project->nodes || !project->ids) return false;

    project->node_capacity = node_cap;
    project->id_capacity   = id_cap;
    return true;
}

const gml_theme_t *gml_project_theme(const gml_project_t *project) {
    return project && project->config ? project->config->theme : NULL;
}

/* ---------------- tree construction --------------------------------------- */

gml_handle_t gml_project_begin_screen(gml_project_t *project, const char *id, int16_t width, int16_t height) {
    if (!project || project->inside_screen) return GML_HANDLE_INVALID;
    gml_handle_t h = allocate_node(project, GML_WIDGET_TYPE_SCREEN, id);
    if (h == GML_HANDLE_INVALID) return h;
    gml_node_t *node = &nodes(project)[h];
    init_widget_for_type(node, project->config->theme, project->config->default_font);
    gui_widget_set_frame(widget_base(node), gui_rect_make(0, 0, width, height));

    project->current_screen = h;
    project->inside_screen = true;
    return h;
}

void gml_project_end_screen(gml_project_t *project) {
    if (!project) return;
    project->current_screen = GML_HANDLE_INVALID;
    project->inside_screen = false;
}

gml_handle_t gml_project_add_widget(gml_project_t *project,
                                    gml_handle_t parent,
                                    const char *id,
                                    gml_widget_type_t type,
                                    gui_rect_t frame) {
    if (!project) return GML_HANDLE_INVALID;
    if (type == GML_WIDGET_TYPE_SCREEN) return GML_HANDLE_INVALID;

    gml_handle_t h = allocate_node(project, type, id);
    if (h == GML_HANDLE_INVALID) return h;
    gml_node_t *node = &nodes(project)[h];
    init_widget_for_type(node, project->config->theme, project->config->default_font);
    gui_widget_set_frame(widget_base(node), frame);
    attach_to_parent(project, node, h, parent);
    return h;
}

/* ---------------- setters ------------------------------------------------- */

#define REQUIRE_NODE(proj, handle, var)                   \
    gml_node_t *var = node_at((proj), (handle));           \
    if (!var) return

void gml_project_set_visible(gml_project_t *p, gml_handle_t h, bool visible) {
    REQUIRE_NODE(p, h, node);
    gui_widget_set_visible(widget_base(node), visible);
}

void gml_project_set_enabled(gml_project_t *p, gml_handle_t h, bool enabled) {
    REQUIRE_NODE(p, h, node);
    widget_base(node)->enabled = enabled;
}

void gml_project_set_frame(gml_project_t *p, gml_handle_t h, gui_rect_t frame) {
    REQUIRE_NODE(p, h, node);
    gui_widget_set_frame(widget_base(node), frame);
}

void gml_project_set_layout(gml_project_t *p, gml_handle_t h, gml_layout_t layout) {
    REQUIRE_NODE(p, h, node);
    if (node->type == GML_WIDGET_TYPE_SCREEN || node->type == GML_WIDGET_TYPE_PANEL) {
        gui_layout_t mode = GUI_LAYOUT_NONE;
        gui_layout_align_t align = GUI_LAYOUT_ALIGN_START;
        gui_layout_align_t justify = GUI_LAYOUT_ALIGN_START;
        switch (layout.mode) {
            case GML_LAYOUT_ROW:    mode = GUI_LAYOUT_ROW;    break;
            case GML_LAYOUT_COLUMN: mode = GUI_LAYOUT_COLUMN; break;
            default:                mode = GUI_LAYOUT_NONE;   break;
        }
        switch (layout.align) {
            case GML_ALIGN_CENTER: align = GUI_LAYOUT_ALIGN_CENTER; break;
            case GML_ALIGN_END: align = GUI_LAYOUT_ALIGN_END; break;
            case GML_ALIGN_STRETCH: align = GUI_LAYOUT_ALIGN_STRETCH; break;
            case GML_ALIGN_SPACE_BETWEEN: align = GUI_LAYOUT_ALIGN_SPACE_BETWEEN; break;
            case GML_ALIGN_START:
            default: align = GUI_LAYOUT_ALIGN_START; break;
        }
        switch (layout.justify) {
            case GML_ALIGN_CENTER: justify = GUI_LAYOUT_ALIGN_CENTER; break;
            case GML_ALIGN_END: justify = GUI_LAYOUT_ALIGN_END; break;
            case GML_ALIGN_STRETCH: justify = GUI_LAYOUT_ALIGN_STRETCH; break;
            case GML_ALIGN_SPACE_BETWEEN: justify = GUI_LAYOUT_ALIGN_SPACE_BETWEEN; break;
            case GML_ALIGN_START:
            default: justify = GUI_LAYOUT_ALIGN_START; break;
        }
        gui_panel_set_layout(&node->storage.panel, mode);
        gui_panel_set_spacing(&node->storage.panel, layout.padding, layout.gap);
        gui_panel_set_align(&node->storage.panel, align, justify);
    }
}

void gml_project_set_style(gml_project_t *p, gml_handle_t h, gml_style_t style) {
    REQUIRE_NODE(p, h, node);
    switch (node->type) {
        case GML_WIDGET_TYPE_SCREEN:
        case GML_WIDGET_TYPE_PANEL:
        case GML_WIDGET_TYPE_IMAGE: {
            gui_panel_t *panel = &node->storage.panel;
            gui_color_t bg = panel->background;
            gui_color_t border = panel->border_color;
            uint8_t border_w = panel->border_width;
            uint8_t radius = panel->radius;
            if (style.has_background)    bg = style.background;
            if (style.has_border_color)  border = style.border_color;
            if (style.has_border_width)  border_w = style.border_width;
            if (style.has_border_radius) radius = style.border_radius;
            gui_panel_set_style(panel, bg, border, border_w);
            gui_panel_set_radius(panel, radius);
            bool draw_bg = panel->draw_background;
            bool draw_bd = panel->draw_border;
            if (style.has_draw_background) draw_bg = style.draw_background;
            if (style.has_draw_border)     draw_bd = style.draw_border;
            gui_panel_set_chrome(panel, draw_bg, draw_bd);
            break;
        }
        case GML_WIDGET_TYPE_RECT:
        case GML_WIDGET_TYPE_CIRCLE:
        case GML_WIDGET_TYPE_TRIANGLE:
        case GML_WIDGET_TYPE_LINE: {
            gui_shape_t *shape = &node->storage.shape;
            gui_color_t fill = shape->fill_color;
            gui_color_t stroke = shape->stroke_color;
            uint8_t stroke_w = shape->stroke_width;
            bool draw_fill = shape->draw_fill;
            bool draw_border = shape->draw_border;
            if (style.has_background) fill = style.background;
            if (style.has_border_color) stroke = style.border_color;
            if (style.has_border_width) stroke_w = style.border_width;
            if (style.has_draw_background) draw_fill = style.draw_background;
            if (style.has_draw_border) draw_border = style.draw_border;
            if (style.has_border_radius && node->type == GML_WIDGET_TYPE_RECT) {
                gui_shape_set_radius(shape, style.border_radius);
            }
            gui_shape_set_style(shape, fill, draw_fill, stroke, stroke_w, draw_border);
            break;
        }
        case GML_WIDGET_TYPE_FREEHAND: {
            gui_freehand_t *freehand = &node->storage.freehand;
            gui_color_t color = freehand->color;
            uint8_t stroke_w = freehand->stroke_width;
            if (style.has_border_color) color = style.border_color;
            else if (style.has_text_color) color = style.text_color;
            if (style.has_border_width) stroke_w = style.border_width;
            gui_freehand_set_style(freehand, color, stroke_w);
            break;
        }
        case GML_WIDGET_TYPE_LABEL:
            if (style.has_text_color) node->storage.label.color = style.text_color;
            break;
        case GML_WIDGET_TYPE_BUTTON: {
            gui_button_t *btn = &node->storage.button;
            gui_button_set_style(btn,
                                 style.has_text_color   ? style.text_color   : btn->text_color,
                                 style.has_background   ? style.background   : btn->background,
                                 btn->pressed_background,
                                 style.has_border_color ? style.border_color : btn->border_color,
                                 style.has_border_width ? style.border_width : btn->border_width);
            if (style.has_border_radius) {
                gui_button_set_radius(btn, style.border_radius);
            }
            bool draw_bg = btn->draw_background;
            bool draw_bd = btn->draw_border;
            if (style.has_draw_background) draw_bg = style.draw_background;
            if (style.has_draw_border) draw_bd = style.draw_border;
            gui_button_set_chrome(btn, draw_bg, draw_bd);
            break;
        }
        case GML_WIDGET_TYPE_ICON:
            if (style.has_text_color) node->storage.icon.color = style.text_color;
            break;
        default:
            break;
    }
}

void gml_project_set_label_text(gml_project_t *p, gml_handle_t h, const char *text) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LABEL) return;
    gui_label_set_text(&node->storage.label, text);
}

void gml_project_set_label_scale(gml_project_t *p, gml_handle_t h, uint8_t scale) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LABEL) return;
    gui_label_set_scale(&node->storage.label, scale);
}

void gml_project_set_label_align(gml_project_t *p, gml_handle_t h, gml_label_align_t align) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LABEL) return;
    gui_label_set_align(&node->storage.label, (uint8_t)align);
}

void gml_project_set_label_vertical_align(gml_project_t *p, gml_handle_t h, gml_vertical_align_t align) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LABEL) return;
    gui_label_set_vertical_align(&node->storage.label, (uint8_t)align);
}

void gml_project_set_label_text_auto_size(gml_project_t *p, gml_handle_t h, bool enabled) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LABEL) return;
    gui_label_set_text_auto_size(&node->storage.label, enabled);
}

void gml_project_set_label_font(gml_project_t *p, gml_handle_t h, const gui_font_t *font) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LABEL || !font) return;
    node->storage.label.font = font;
    gui_widget_request_layout(widget_base(node));
    gui_widget_invalidate(widget_base(node));
}

void gml_project_set_button_text(gml_project_t *p, gml_handle_t h, const char *text) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    node->storage.button.text = text ? text : "";
    gui_widget_invalidate(widget_base(node));
}

void gml_project_set_button_font(gml_project_t *p, gml_handle_t h, const gui_font_t *font) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON || !font) return;
    node->storage.button.font = font;
    gui_widget_request_layout(widget_base(node));
    gui_widget_invalidate(widget_base(node));
}

void gml_project_set_button_pressed_bg(gml_project_t *p, gml_handle_t h, gui_color_t color) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    node->storage.button.pressed_background = color;
}

void gml_project_set_button_padding(gml_project_t *p, gml_handle_t h, uint8_t px, uint8_t py) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    gui_button_set_padding(&node->storage.button, px, py);
}

void gml_project_set_button_padding_sides(gml_project_t *p,
                                          gml_handle_t h,
                                          uint8_t top,
                                          uint8_t right,
                                          uint8_t bottom,
                                          uint8_t left) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    gui_button_set_padding_sides(&node->storage.button, top, right, bottom, left);
}

void gml_project_set_button_scale(gml_project_t *p, gml_handle_t h, uint8_t scale) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    gui_button_set_text_scale(&node->storage.button, scale);
}

void gml_project_set_button_icon(gml_project_t *p, gml_handle_t h, const char *icon_id) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    gui_button_set_icon(&node->storage.button, find_icon_asset(p, icon_id));
}

void gml_project_set_button_icon_layout(gml_project_t *p,
                                        gml_handle_t h,
                                        const char *position,
                                        uint8_t gap) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    gui_button_icon_position_t icon_position = GUI_BUTTON_ICON_POSITION_LEFT;
    if (position != NULL) {
        if (strcmp(position, "right") == 0) icon_position = GUI_BUTTON_ICON_POSITION_RIGHT;
        else if (strcmp(position, "top") == 0) icon_position = GUI_BUTTON_ICON_POSITION_TOP;
        else if (strcmp(position, "bottom") == 0) icon_position = GUI_BUTTON_ICON_POSITION_BOTTOM;
    }
    gui_button_set_icon_layout(&node->storage.button, icon_position, gap);
}

void gml_project_set_button_content_align(gml_project_t *p,
                                          gml_handle_t h,
                                          gml_label_align_t horizontal,
                                          gml_vertical_align_t vertical) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_BUTTON) return;
    gui_button_set_content_align(&node->storage.button,
                                 (gui_button_align_t)horizontal,
                                 (gui_button_vertical_align_t)vertical);
}

void gml_project_set_icon_asset_id(gml_project_t *p, gml_handle_t h, const char *icon_id) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_ICON) return;
    gui_icon_widget_set_icon(&node->storage.icon, find_icon_asset(p, icon_id));
}

void gml_project_set_icon_color(gml_project_t *p, gml_handle_t h, gui_color_t color) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_ICON) return;
    node->storage.icon.color = color;
    gui_widget_invalidate(widget_base(node));
}

void gml_project_set_shape_radius(gml_project_t *p, gml_handle_t h, uint8_t radius) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_RECT && node->type != GML_WIDGET_TYPE_CIRCLE) return;
    gui_shape_set_radius(&node->storage.shape, radius);
}

void gml_project_set_shape_triangle_direction(gml_project_t *p,
                                              gml_handle_t h,
                                              gml_triangle_direction_t direction) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_TRIANGLE) return;
    gui_shape_triangle_direction_t dir = GUI_SHAPE_TRIANGLE_UP;
    switch (direction) {
        case GML_TRIANGLE_DIRECTION_RIGHT: dir = GUI_SHAPE_TRIANGLE_RIGHT; break;
        case GML_TRIANGLE_DIRECTION_DOWN: dir = GUI_SHAPE_TRIANGLE_DOWN; break;
        case GML_TRIANGLE_DIRECTION_LEFT: dir = GUI_SHAPE_TRIANGLE_LEFT; break;
        case GML_TRIANGLE_DIRECTION_UP:
        default: dir = GUI_SHAPE_TRIANGLE_UP; break;
    }
    gui_shape_set_triangle_direction(&node->storage.shape, dir);
}

void gml_project_set_shape_line(gml_project_t *p,
                                gml_handle_t h,
                                int16_t x1,
                                int16_t y1,
                                int16_t x2,
                                int16_t y2,
                                uint8_t stroke_width) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_LINE) return;
    gui_shape_set_line(&node->storage.shape, x1, y1, x2, y2, stroke_width);
}

void gml_project_set_freehand_points(gml_project_t *p,
                                     gml_handle_t h,
                                     const gui_point_t *points,
                                     uint16_t point_count,
                                     uint8_t stroke_width) {
    REQUIRE_NODE(p, h, node);
    if (node->type != GML_WIDGET_TYPE_FREEHAND) return;
    gui_freehand_set_points(&node->storage.freehand, points, point_count);
    gui_freehand_set_style(&node->storage.freehand, node->storage.freehand.color, stroke_width);
}

void gml_project_set_rotation(gml_project_t *p, gml_handle_t h, int16_t rotation_degrees) {
    REQUIRE_NODE(p, h, node);
    gui_widget_set_rotation(widget_base(node), rotation_degrees);
}

void gml_project_set_bindings(gml_project_t *p,
                              gml_handle_t h,
                              const gml_binding_t *bindings,
                              uint16_t count) {
    REQUIRE_NODE(p, h, node);
    node->bindings = NULL;
    node->binding_count = 0;
    if (bindings == NULL || count == 0) {
        return;
    }
    gml_binding_t *stored = arena_alloc(p, sizeof(gml_binding_t) * count, _Alignof(gml_binding_t));
    if (stored == NULL) {
        return;
    }
    memcpy(stored, bindings, sizeof(gml_binding_t) * count);
    node->bindings = stored;
    node->binding_count = count;
}

void gml_project_set_on_press(gml_project_t *p,
                              gml_handle_t h,
                              gml_action_kind_t kind,
                              const char *target_id,
                              const char *value) {
    REQUIRE_NODE(p, h, node);
    node->on_press_kind   = kind;
    node->on_press_target = target_id;
    node->on_press_value  = value;
}

/* ---------------- lookup -------------------------------------------------- */

gml_handle_t gml_project_find(const gml_project_t *project, const char *id) {
    if (!project) return GML_HANDLE_INVALID;
    return lookup_id(project, id);
}

gui_widget_t *gml_project_widget(const gml_project_t *project, gml_handle_t handle) {
    gml_node_t *node = node_at(project, handle);
    return node ? widget_base(node) : NULL;
}

gml_widget_type_t gml_project_widget_type(const gml_project_t *project, gml_handle_t handle) {
    gml_node_t *node = node_at(project, handle);
    return node ? node->type : GML_WIDGET_TYPE__COUNT;
}

/* ---------------- runtime actions ----------------------------------------- */

bool gml_project_show_screen(gml_project_t *project, const char *screen_id) {
    if (!project) return false;
    gml_handle_t target = lookup_id(project, screen_id);
    gml_node_t *screen = node_at(project, target);
    if (!screen || screen->type != GML_WIDGET_TYPE_SCREEN) return false;

    for (uint16_t i = 0; i < project->node_count; ++i) {
        gml_node_t *node = &nodes(project)[i];
        if (node->type == GML_WIDGET_TYPE_SCREEN) {
            gui_widget_set_visible(widget_base(node), i == target);
        }
    }

    project->active_screen = target;
    if (project->config && project->config->context) {
        gui_context_set_root(project->config->context, widget_base(screen));
    }
    return true;
}

const char *gml_project_active_screen_id(const gml_project_t *project) {
    if (!project || project->active_screen == GML_HANDLE_INVALID) return NULL;
    gml_node_t *node = &nodes(project)[project->active_screen];
    return node->id;
}

bool gml_project_dispatch_action(gml_project_t *project,
                                 gml_action_kind_t kind,
                                 const char *target_id,
                                 const char *value) {
    if (!project) return false;
    switch (kind) {
        case GML_ACTION_NAVIGATE:
            return gml_project_show_screen(project, target_id);
        case GML_ACTION_SHOW: {
            gml_handle_t h = lookup_id(project, target_id);
            gui_widget_t *w = gml_project_widget(project, h);
            if (!w) return false;
            gui_widget_set_visible(w, true);
            return true;
        }
        case GML_ACTION_HIDE: {
            gml_handle_t h = lookup_id(project, target_id);
            gui_widget_t *w = gml_project_widget(project, h);
            if (!w) return false;
            gui_widget_set_visible(w, false);
            return true;
        }
        case GML_ACTION_SET_TEXT: {
            gml_handle_t h = lookup_id(project, target_id);
            gml_project_set_label_text(project, h, value);
            return true;
        }
        case GML_ACTION_SET_VALUE:
            return gml_project_dispatch_action(project, GML_ACTION_SET_TEXT, target_id, value);
        case GML_ACTION_NONE:
        default:
            return false;
    }
}
