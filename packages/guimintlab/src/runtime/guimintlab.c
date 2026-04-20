/*
 * GuiMintLab public runtime.
 *
 * This file implements the guimintlab public API by wiring together:
 *   - the display driver + context (src/gui/...)
 *   - the declarative builder (src/internal/guimintlab_builder.c/.h)
 *   - the IR JSON loader (src/loader/guimintlab_loader.c/.h)
 *
 * The idea is that application code depends ONLY on <guimintlab/guimintlab.h>
 * and a project.json blob — everything else lives inside this component.
 */

#include "guimintlab/guimintlab.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gui/core/gui_context.h"
#include "gui/display/gui_display.h"
#include "gui/theme/gui_theme.h"
#include "gui/generated/gui_assets_generated.h"
#include "gui/widgets/gui_button.h"

#include "guimintlab_builder.h"
#include "guimintlab_runtime_internal.h"
#include "guimintlab_schema.h"
#include "guimintlab_loader.h"

static const char *TAG = "guimintlab";

#define GML_DEFAULT_MAX_WIDGETS 64
#define GML_DEFAULT_TICK_DELAY_MS 25

/* Forward decl: implemented below; resolves the owning runtime from a press
 * entry pointer via the small static registry. */
static guimintlab_t *gml_runtime_owner_of_entry(gml_press_entry_t *e);

/* Arena needs to hold widget nodes + id table + growable storage inside the
 * builder. This bound is empirical — it assumes every node costs roughly
 * sizeof(gui_panel_t) + sizeof(id entry). Grow if you start hitting limits. */
static size_t arena_bytes_for(uint16_t max_widgets) {
    /* 160 bytes per node is generous; actual worst-case is ~128 for panels. */
    size_t per_node = 256;
    size_t base = 512;
    return base + (size_t)max_widgets * per_node;
}

/* ---------------------------------------------------------------------------
 * Internal helpers exposed to the loader.
 * --------------------------------------------------------------------------*/

const char *gml_runtime_strpool_put(guimintlab_t *gml, const char *s) {
    if (gml == NULL || s == NULL) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    if (gml->strpool_used + n > gml->strpool_size) {
        ESP_LOGW(TAG, "strpool exhausted (need %zu, have %zu)",
                 n, gml->strpool_size - gml->strpool_used);
        return NULL;
    }
    char *dst = gml->strpool + gml->strpool_used;
    memcpy(dst, s, n);
    gml->strpool_used += n;
    return dst;
}

gml_press_entry_t *gml_runtime_alloc_press_entry(guimintlab_t *gml, const char *widget_id) {
    if (gml->press_entry_count >= gml->press_entry_capacity) {
        return NULL;
    }
    gml_press_entry_t *e = &gml->press_entries[gml->press_entry_count++];
    memset(e, 0, sizeof(*e));
    e->widget_id = widget_id;
    return e;
}

static gml_press_entry_t *find_press_entry(const guimintlab_t *gml, const char *widget_id) {
    if (widget_id == NULL) {
        return NULL;
    }
    for (uint16_t i = 0; i < gml->press_entry_count; ++i) {
        gml_press_entry_t *e = &((guimintlab_t *)gml)->press_entries[i];
        if (e->widget_id != NULL && strcmp(e->widget_id, widget_id) == 0) {
            return e;
        }
    }
    return NULL;
}

void gml_runtime_internal_button_click(void *user_data) {
    gml_press_entry_t *e = (gml_press_entry_t *)user_data;
    if (e == NULL) {
        return;
    }
    /* Resolve the enclosing guimintlab_t. Entries live inside gml->press_entries,
     * but we don't keep a back-pointer — instead we stashed it via on_press(). */
    /* For simplicity we use the strpool invariant: action_target / value live
     * inside gml, and press callbacks carry their own user_data pointing here.
     * We get the gml via a small trick: the entries array is flat, entry-0 is
     * at a known offset before the gml's other fields. Instead of pointer
     * arithmetic, keep it simple by stashing gml in user_cb.user_data when
     * caller uses on_press(). For the IR dispatch path we need a pointer. We
     * resolve it by walking up through the gml list kept by this module. */

    /* We keep a small list of active runtimes so trampolines can find the
     * owning guimintlab_t from an entry pointer. */
    guimintlab_t *gml = gml_runtime_owner_of_entry(e);
    if (gml == NULL) {
        return;
    }

    if (e->user_cb != NULL) {
        e->user_cb(gml, e->widget_id, e->user_data);
        return;
    }
    if (e->action_kind != GML_ACTION_NONE) {
        gml_project_dispatch_action(&gml->project, e->action_kind,
                                    e->action_target, e->action_value);
    }
}

/* ---------------------------------------------------------------------------
 * Active runtime registry — tiny static list so trampolines can resolve their
 * owning guimintlab_t from a press-entry pointer.
 * --------------------------------------------------------------------------*/

#define GML_REGISTRY_MAX 4
static guimintlab_t *s_registry[GML_REGISTRY_MAX];

static void registry_add(guimintlab_t *gml) {
    for (int i = 0; i < GML_REGISTRY_MAX; ++i) {
        if (s_registry[i] == NULL) {
            s_registry[i] = gml;
            return;
        }
    }
    ESP_LOGE(TAG, "registry full; too many concurrent guimintlab_t instances");
}

static void registry_remove(guimintlab_t *gml) {
    for (int i = 0; i < GML_REGISTRY_MAX; ++i) {
        if (s_registry[i] == gml) {
            s_registry[i] = NULL;
            return;
        }
    }
}

static guimintlab_t *gml_runtime_owner_of_entry(gml_press_entry_t *e) {
    for (int i = 0; i < GML_REGISTRY_MAX; ++i) {
        guimintlab_t *g = s_registry[i];
        if (g == NULL) continue;
        if (e >= g->press_entries && e < g->press_entries + g->press_entry_capacity) {
            return g;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Project lifecycle.
 * --------------------------------------------------------------------------*/

static esp_err_t init_builder(guimintlab_t *gml) {
    uint16_t max_widgets = gml->cfg.max_widgets ? gml->cfg.max_widgets : GML_DEFAULT_MAX_WIDGETS;
    uint16_t max_ids     = gml->cfg.max_ids     ? gml->cfg.max_ids     : max_widgets;

    size_t arena_size = gml->cfg.arena_size ? gml->cfg.arena_size : arena_bytes_for(max_widgets);
    if (gml->project_arena != NULL && gml->project_arena_size != arena_size) {
        free(gml->project_arena);
        gml->project_arena = NULL;
    }
    if (gml->project_arena == NULL) {
        gml->project_arena = heap_caps_malloc(arena_size, MALLOC_CAP_8BIT);
        if (gml->project_arena == NULL) {
            return ESP_ERR_NO_MEM;
        }
        gml->project_arena_size = arena_size;
    }

    gml->project_cfg = (gml_project_config_t){
        .arena        = gml->project_arena,
        .arena_size   = gml->project_arena_size,
        .theme        = &gml->theme,
        .context      = &gml->context,
        .max_nodes    = max_widgets,
        .max_ids      = max_ids,
        .default_font = gml->theme.font_body,
    };
    if (!gml_project_init(&gml->project, &gml->project_cfg)) {
        ESP_LOGE(TAG, "gml_project_init failed (arena too small?)");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void reset_runtime_tables(guimintlab_t *gml) {
    gml->strpool_used = 0;
    gml->press_entry_count = 0;
    if (gml->press_entries != NULL) {
        memset(gml->press_entries, 0,
               sizeof(*gml->press_entries) * gml->press_entry_capacity);
    }
}

esp_err_t guimintlab_new(const guimintlab_config_t *cfg, guimintlab_t **out) {
    if (cfg == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    guimintlab_t *gml = heap_caps_calloc(1, sizeof(*gml), MALLOC_CAP_8BIT);
    if (gml == NULL) {
        return ESP_ERR_NO_MEM;
    }
    gml->cfg = *cfg;

    uint16_t max_widgets = gml->cfg.max_widgets ? gml->cfg.max_widgets : GML_DEFAULT_MAX_WIDGETS;
    gml->press_entry_capacity = max_widgets;
    gml->press_entries = heap_caps_calloc(max_widgets, sizeof(*gml->press_entries),
                                          MALLOC_CAP_8BIT);
    gml->strpool_size  = (size_t)max_widgets * 48; /* ~48 bytes/widget of strings */
    gml->strpool       = heap_caps_malloc(gml->strpool_size, MALLOC_CAP_8BIT);
    if (gml->press_entries == NULL || gml->strpool == NULL) {
        goto oom;
    }

    const gui_display_config_t display_config = {
        .host              = cfg->display.host,
        .pin_mosi          = cfg->display.pin_mosi,
        .pin_sclk          = cfg->display.pin_sclk,
        .pin_cs            = cfg->display.pin_cs,
        .pin_dc            = cfg->display.pin_dc,
        .pin_rst           = cfg->display.pin_rst,
        .pin_bl            = cfg->display.pin_bl,
        .backlight_on_level = cfg->display.backlight_on_level,
        .width             = cfg->display.width,
        .height            = cfg->display.height,
        .spi_clock_hz      = cfg->display.spi_clock_hz,
        .transfer_rows     = cfg->display.transfer_rows > 0 ? (size_t)cfg->display.transfer_rows : 20,
        .swap_xy           = cfg->display.swap_xy,
        .mirror_x          = cfg->display.mirror_x,
        .mirror_y          = cfg->display.mirror_y,
    };
    esp_err_t err = gui_display_init(&gml->display, &display_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gui_display_init failed: %s", esp_err_to_name(err));
        goto fail;
    }
    err = gui_context_init(&gml->context, &gml->display);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gui_context_init failed: %s", esp_err_to_name(err));
        goto fail;
    }
    gui_theme_init_default(&gml->theme);

    err = init_builder(gml);
    if (err != ESP_OK) {
        goto fail;
    }

    registry_add(gml);
    *out = gml;
    return ESP_OK;

oom:
    err = ESP_ERR_NO_MEM;
fail:
    guimintlab_free(gml);
    return err;
}

void guimintlab_free(guimintlab_t *gml) {
    if (gml == NULL) {
        return;
    }
    registry_remove(gml);
    free(gml->project_arena);
    free(gml->press_entries);
    free(gml->strpool);
    /* display + context teardown is intentionally omitted — gui_display_t has
     * no public deinit today; add one in gui_display.c if this ever matters. */
    free(gml);
}

esp_err_t guimintlab_load_from_memory(guimintlab_t *gml, const void *json, size_t size) {
    if (gml == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    reset_runtime_tables(gml);
    esp_err_t err = init_builder(gml);
    if (err != ESP_OK) {
        return err;
    }
    err = gml_loader_load_from_memory(gml, json, size);
    if (err == ESP_OK) {
        gml->loaded = true;
        gui_context_render(&gml->context);
    }
    return err;
}

esp_err_t guimintlab_show_screen(guimintlab_t *gml, const char *screen_id) {
    if (gml == NULL || screen_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!gml_project_show_screen(&gml->project, screen_id)) {
        return ESP_ERR_NOT_FOUND;
    }
    gui_context_render(&gml->context);
    return ESP_OK;
}

guimintlab_widget_t guimintlab_find(const guimintlab_t *gml, const char *widget_id) {
    if (gml == NULL || widget_id == NULL) {
        return GUIMINTLAB_WIDGET_INVALID;
    }
    gml_handle_t h = gml_project_find(&gml->project, widget_id);
    return (h == GML_HANDLE_INVALID) ? GUIMINTLAB_WIDGET_INVALID : (guimintlab_widget_t)h;
}

esp_err_t guimintlab_set_text(guimintlab_t *gml, const char *widget_id, const char *text) {
    if (gml == NULL || widget_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    gml_handle_t h = gml_project_find(&gml->project, widget_id);
    if (h == GML_HANDLE_INVALID) {
        return ESP_ERR_NOT_FOUND;
    }
    const char *stored = gml_runtime_strpool_put(gml, text != NULL ? text : "");
    if (stored == NULL) {
        return ESP_ERR_NO_MEM;
    }
    gml_widget_type_t type = gml_project_widget_type(&gml->project, h);
    if (type == GML_WIDGET_TYPE_LABEL) {
        gml_project_set_label_text(&gml->project, h, stored);
    } else if (type == GML_WIDGET_TYPE_BUTTON) {
        gml_project_set_button_text(&gml->project, h, stored);
    } else {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t guimintlab_set_visible(guimintlab_t *gml, const char *widget_id, bool visible) {
    if (gml == NULL || widget_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    gml_handle_t h = gml_project_find(&gml->project, widget_id);
    if (h == GML_HANDLE_INVALID) {
        return ESP_ERR_NOT_FOUND;
    }
    gml_project_set_visible(&gml->project, h, visible);
    return ESP_OK;
}

esp_err_t guimintlab_on_press(guimintlab_t *gml,
                              const char *widget_id,
                              guimintlab_press_cb_t cb,
                              void *user_data)
{
    if (gml == NULL || widget_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    gml_handle_t h = gml_project_find(&gml->project, widget_id);
    if (h == GML_HANDLE_INVALID) {
        return ESP_ERR_NOT_FOUND;
    }

    const char *stored_id = gml_runtime_strpool_put(gml, widget_id);
    if (stored_id == NULL) {
        return ESP_ERR_NO_MEM;
    }

    gml_press_entry_t *entry = find_press_entry(gml, widget_id);
    if (entry == NULL) {
        entry = gml_runtime_alloc_press_entry(gml, stored_id);
        if (entry == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    entry->user_cb = cb;
    entry->user_data = user_data;

    /* Ensure the widget is wired to the trampoline (loader may not have done
     * it if the IR had no onPress). */
    if (gml_project_widget_type(&gml->project, h) == GML_WIDGET_TYPE_BUTTON) {
        gui_widget_t *w = gml_project_widget(&gml->project, h);
        if (w != NULL) {
            gui_button_set_on_click((gui_button_t *)w,
                                    gml_runtime_internal_button_click, entry);
        }
    }
    return ESP_OK;
}

esp_err_t guimintlab_tick(guimintlab_t *gml) {
    if (gml == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    gui_context_render(&gml->context);
    return ESP_OK;
}

void guimintlab_run(guimintlab_t *gml) {
    if (gml == NULL) {
        return;
    }
    for (;;) {
        gui_context_render(&gml->context);
        vTaskDelay(pdMS_TO_TICKS(GML_DEFAULT_TICK_DELAY_MS));
    }
}
