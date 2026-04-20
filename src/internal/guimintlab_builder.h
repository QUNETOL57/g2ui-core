#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gui/core/gui_context.h"
#include "gui/core/gui_types.h"
#include "gui/core/gui_widget.h"
#include "gui/theme/gui_theme.h"

#include "guimintlab_schema.h"

/**
 * GuiMintLab declarative builder API.
 *
 * Generated C code (produced by `@guimintlab/ui-codegen-c`) ONLY speaks this
 * API. Hand-written firmware code may use it too, but ad-hoc widget
 * allocation is the old path and must be avoided for anything that comes
 * from a project.guimintlab.json.
 *
 * Arena model:
 *   - the caller provides a byte arena sized for all widgets and internal
 *     book-keeping of a project;
 *   - the builder allocates widget storage and an id->handle table from it;
 *   - there is no free: lifetime == project lifetime.
 */

typedef uint16_t gml_handle_t;
#define GML_HANDLE_INVALID ((gml_handle_t)0xFFFFu)

typedef struct gml_project_t gml_project_t;
typedef gui_theme_t gml_theme_t;

static inline void gml_theme_init_default(gml_theme_t *theme) {
    gui_theme_init_default(theme);
}

typedef struct {
    uint8_t *arena;
    size_t   arena_size;
    const gml_theme_t *theme;
    gui_context_t *context;
    /** Max number of widget nodes (including screens). 0 = derive from arena. */
    uint16_t max_nodes;
    /** Max id table entries. 0 = same as max_nodes. */
    uint16_t max_ids;
    /** Optional default font for labels/buttons if screen/widget omits it. */
    const gui_font_t *default_font;
    /** Optional icon + image registries for icon/image widgets. NULL -> lookups fail gracefully. */
    const struct gml_icon_registry_t *icons;
    const struct gml_image_registry_t *images;
} gml_project_config_t;

typedef struct gml_icon_registry_entry_t {
    const char *icon_id;
    const gui_icon_asset_t *asset;
} gml_icon_registry_entry_t;

typedef struct gml_icon_registry_t {
    const gml_icon_registry_entry_t *entries;
    size_t count;
} gml_icon_registry_t;

typedef struct gml_image_registry_entry_t {
    const char *image_id;
    const gui_image_asset_t *asset;
} gml_image_registry_entry_t;

typedef struct gml_image_registry_t {
    const gml_image_registry_entry_t *entries;
    size_t count;
} gml_image_registry_t;

typedef struct {
    bool has_background;   gui_color_t background;
    bool has_border_color; gui_color_t border_color;
    bool has_text_color;   gui_color_t text_color;
    bool has_border_width; uint8_t     border_width;
    bool has_draw_background; bool     draw_background;
    bool has_draw_border;     bool     draw_border;
} gml_style_t;

typedef struct {
    gml_layout_mode_t mode;
    uint8_t padding;
    uint8_t gap;
    gml_align_t align;
    gml_align_t justify;
} gml_layout_t;

/* ----- lifecycle ---------------------------------------------------------- */

bool gml_project_init(gml_project_t *project, const gml_project_config_t *config);

/* Returns the theme passed in config(). Convenience for style helpers. */
const gml_theme_t *gml_project_theme(const gml_project_t *project);

/* ----- tree construction -------------------------------------------------- */

/**
 * Declare a screen.
 *  - `id` must be a stable, unique, nul-terminated string (points into static storage).
 *  - `width/height` come from IR display spec.
 *  - returns handle to the screen widget, or GML_HANDLE_INVALID on failure.
 */
gml_handle_t gml_project_begin_screen(gml_project_t *project,
                                      const char *id,
                                      int16_t width,
                                      int16_t height);

/**
 * Finish the current screen. Every begin_screen must be matched.
 */
void gml_project_end_screen(gml_project_t *project);

/**
 * Append a widget. `parent` must be a handle returned by begin_screen or
 * add_widget; pass GML_HANDLE_INVALID as parent if you want to add to the
 * current screen root.
 */
gml_handle_t gml_project_add_widget(gml_project_t *project,
                                    gml_handle_t parent,
                                    const char *id,
                                    gml_widget_type_t type,
                                    gui_rect_t frame);

/* ----- property setters (by handle) --------------------------------------- */

void gml_project_set_visible(gml_project_t *project, gml_handle_t handle, bool visible);
void gml_project_set_enabled(gml_project_t *project, gml_handle_t handle, bool enabled);
void gml_project_set_frame(gml_project_t *project, gml_handle_t handle, gui_rect_t frame);
void gml_project_set_layout(gml_project_t *project, gml_handle_t handle, gml_layout_t layout);
void gml_project_set_style(gml_project_t *project, gml_handle_t handle, gml_style_t style);

/* Type-specific setters (no-op if handle is not of that type) */
void gml_project_set_label_text(gml_project_t *project, gml_handle_t handle, const char *text);
void gml_project_set_label_scale(gml_project_t *project, gml_handle_t handle, uint8_t scale);
void gml_project_set_label_align(gml_project_t *project, gml_handle_t handle, gml_label_align_t align);
void gml_project_set_label_font(gml_project_t *project, gml_handle_t handle, const gui_font_t *font);

void gml_project_set_button_text(gml_project_t *project, gml_handle_t handle, const char *text);
void gml_project_set_button_pressed_bg(gml_project_t *project, gml_handle_t handle, gui_color_t color);
void gml_project_set_button_padding(gml_project_t *project, gml_handle_t handle, uint8_t px, uint8_t py);

void gml_project_set_icon_asset_id(gml_project_t *project, gml_handle_t handle, const char *icon_id);
void gml_project_set_icon_color(gml_project_t *project, gml_handle_t handle, gui_color_t color);

void gml_project_set_on_press(gml_project_t *project,
                              gml_handle_t handle,
                              gml_action_kind_t kind,
                              const char *target_id,
                              const char *value);

/* ----- id lookup ---------------------------------------------------------- */

gml_handle_t gml_project_find(const gml_project_t *project, const char *id);
gui_widget_t *gml_project_widget(const gml_project_t *project, gml_handle_t handle);
gml_widget_type_t gml_project_widget_type(const gml_project_t *project, gml_handle_t handle);

/* ----- runtime actions ---------------------------------------------------- */

/** Make the given screen active and request a redraw. */
bool gml_project_show_screen(gml_project_t *project, const char *screen_id);
const char *gml_project_active_screen_id(const gml_project_t *project);

/** Apply an action (keyed by ids) — used both by on_press and by external code. */
bool gml_project_dispatch_action(gml_project_t *project,
                                 gml_action_kind_t kind,
                                 const char *target_id,
                                 const char *value);

/* ----- opaque project definition ----------------------------------------- */

struct gml_project_t {
    const gml_project_config_t *config;
    uint8_t *arena;
    size_t   arena_size;
    size_t   arena_used;

    void *nodes;              /* gml_node_t* -- opaque to public API */
    uint16_t node_count;
    uint16_t node_capacity;

    void *ids;                /* id table */
    uint16_t id_count;
    uint16_t id_capacity;

    gml_handle_t current_screen;
    gml_handle_t active_screen;
    bool inside_screen;
};
