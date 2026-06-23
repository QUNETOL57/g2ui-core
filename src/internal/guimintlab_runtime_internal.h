#pragma once

/*
 * Internal runtime state — the concrete definition of guimintlab_s.
 * Not exposed to library users.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "guimintlab/guimintlab.h"

#include "gui/core/gui_context.h"
#include "gui/core/gui_types.h"
#include "gui/display/gui_display.h"
#include "gui/theme/gui_theme.h"

#include "guimintlab_builder.h"
#include "guimintlab_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-pressable-widget bookkeeping: what the loader found for its onPress,
 * plus optional user override. Stored in a flat array sized to max_widgets. */
typedef struct {
    const char *widget_id;        /* strpool pointer; NULL = unused slot  */
    gml_action_kind_t action_kind;
    const char *action_target;    /* strpool pointer or NULL              */
    const char *action_value;     /* strpool pointer or NULL              */

    guimintlab_press_cb_t user_cb;
    void *user_data;
} gml_press_entry_t;

struct guimintlab_s {
    guimintlab_config_t cfg;

    /* Display + GUI core ---------------------------------------------------*/
    gui_display_t display;
    gui_context_t context;
    gui_theme_t   theme;

    /* Builder project ------------------------------------------------------*/
    gml_project_t         project;
    gml_project_config_t  project_cfg;   /* persistent storage; project->config points here */
    uint8_t              *project_arena;
    size_t                project_arena_size;

    /* String pool — holds all strings pulled out of the loaded JSON IR so
     * that widget ids / text / action targets survive after we free cJSON. */
    char   *strpool;
    size_t  strpool_size;
    size_t  strpool_used;
    gui_point_t *point_pool;
    uint16_t point_pool_capacity;
    uint16_t point_pool_used;

    /* Press bookkeeping ----------------------------------------------------*/
    gml_press_entry_t *press_entries;
    uint16_t           press_entry_count;
    uint16_t           press_entry_capacity;

    bool loaded;
};

/* Helpers implemented in guimintlab.c and used by the loader. */
const char *gml_runtime_strpool_put(guimintlab_t *gml, const char *s);
gml_press_entry_t *gml_runtime_alloc_press_entry(guimintlab_t *gml, const char *widget_id);
gui_point_t *gml_runtime_alloc_points(guimintlab_t *gml, uint16_t point_count);
void gml_runtime_internal_button_click(void *user_data);

#ifdef __cplusplus
}
#endif
