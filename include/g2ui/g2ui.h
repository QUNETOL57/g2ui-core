#pragma once

/*
 * G2UI runtime — public API.
 *
 * Usage:
 *   1. g2ui_new(&cfg, &gml);              // init display + context + empty project
 *   2. g2ui_load_from_memory(gml, ...);   // parse project JSON, build widget tree
 *   3. g2ui_show_screen(gml, "main");     // pick initial screen
 *   4. g2ui_run(gml);                     // drive the render loop
 *
 * Anywhere after load, you can:
 *   - g2ui_find(gml, "btn_1")             -> widget handle
 *   - g2ui_set_text(gml, "btn_1", "...")
 *   - g2ui_set_text_color(gml, "lab_1", 0xFF8702)
 *   - g2ui_set_visible(gml, "panel", false)
 *   - g2ui_on_press(gml, "btn_1", cb, user)
 *
 * All heavy state is heap-allocated; user keeps an opaque pointer.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Opaque runtime handle.
 * --------------------------------------------------------------------------*/
typedef struct g2ui_s g2ui_t;

/*
 * Widget handle — a lightweight index into the internal node table.
 * Handles stay valid for the lifetime of a loaded project; reloading a project
 * invalidates all handles.
 */
typedef uint16_t g2ui_widget_t;
#define G2UI_WIDGET_INVALID ((g2ui_widget_t)0xFFFF)

/* ---------------------------------------------------------------------------
 * Display configuration (thin wrapper over the internal SPI LCD driver).
 * Keeps the user from needing to pull any private headers just to wire a TFT.
 * --------------------------------------------------------------------------*/
typedef struct {
    int     host;              /* SPI host, e.g. SPI2_HOST */
    int     pin_mosi;
    int     pin_sclk;
    int     pin_cs;
    int     pin_dc;
    int     pin_rst;
    int     pin_bl;            /* backlight; -1 if not controlled */
    bool    backlight_on_level;
    int     width;
    int     height;
    int     spi_clock_hz;
    int     transfer_rows;     /* chunk size for flush; 20 is a good default */
    bool    swap_xy;
    bool    mirror_x;
    bool    mirror_y;
    int     gap_x;          /* column offset in ST7789 internal memory */
    int     gap_y;          /* row offset in ST7789 internal memory */
    bool    pre_invert;     /* pre-invert pixel data for displays with INVON hardwired */
} g2ui_display_config_t;

typedef struct {
    g2ui_display_config_t display;

    /* Runtime limits. Zero means "use defaults". */
    uint16_t max_widgets;   /* default 64 */
    uint16_t max_ids;       /* default = max_widgets */
    uint16_t max_freehand_points; /* default = max_widgets * 64 */

    /* Optional: internal bookkeeping arena size. Zero means auto-sized from max_widgets. */
    size_t   arena_size;
} g2ui_config_t;

/* Signature for a press callback. widget_id is the stable string id from IR. */
typedef void (*g2ui_press_cb_t)(g2ui_t *gml, const char *widget_id, void *user_data);

/* ---------------------------------------------------------------------------
 * Lifecycle.
 * --------------------------------------------------------------------------*/

/* Allocate and initialise a runtime with the given display config.
 * The returned pointer is owned by the caller and must be released via
 * g2ui_free(). Returns ESP_OK on success. */
esp_err_t g2ui_new(const g2ui_config_t *cfg, g2ui_t **out);

/* Release all resources: display, framebuffer, widget tree. Safe on NULL. */
void g2ui_free(g2ui_t *gml);

/* ---------------------------------------------------------------------------
 * Project loading.
 *
 * The project JSON follows the IR shape documented in g2ui-core:
 *   { schemaVersion, id, display, palette, initialScreenId, screens: [...] }
 *
 * After a successful load the runtime holds a fully built widget tree and
 * honours the project's initialScreenId. You can still call show_screen to
 * override.
 * --------------------------------------------------------------------------*/

/* Parse a JSON buffer (UTF-8, not zero-terminated required). Rebuilds the
 * project from scratch — previous widgets and handles are discarded. */
esp_err_t g2ui_load_from_memory(g2ui_t *gml, const void *json, size_t size);

/* ---------------------------------------------------------------------------
 * Screens and widget manipulation.
 * All setters match on the stable string id from IR ("btn_1", "header", ...).
 * --------------------------------------------------------------------------*/

esp_err_t g2ui_show_screen(g2ui_t *gml, const char *screen_id);

/* Returns G2UI_WIDGET_INVALID if no widget with that id exists. */
g2ui_widget_t g2ui_find(const g2ui_t *gml, const char *widget_id);

esp_err_t g2ui_set_text(g2ui_t *gml, const char *widget_id, const char *text);

/* Set label text color. rgb is 0xRRGGBB (e.g. 0xFF8702). */
esp_err_t g2ui_set_text_color(g2ui_t *gml, const char *widget_id, uint32_t rgb);

esp_err_t g2ui_set_visible(g2ui_t *gml, const char *widget_id, bool visible);

esp_err_t g2ui_on_press(g2ui_t *gml,
                              const char *widget_id,
                              g2ui_press_cb_t cb,
                              void *user_data);

/* ---------------------------------------------------------------------------
 * Render loop.
 *
 * Two styles are available, pick whichever fits the caller:
 *
 *   g2ui_tick(gml)  — flushes dirty regions once; call from your own loop.
 *   g2ui_run(gml)   — blocks forever, ticking at ~40 Hz with vTaskDelay.
 * --------------------------------------------------------------------------*/

esp_err_t g2ui_tick(g2ui_t *gml);
void      g2ui_run(g2ui_t *gml);

#ifdef __cplusplus
}
#endif
