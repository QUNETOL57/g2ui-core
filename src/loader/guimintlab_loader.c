/*
 * GuiMintLab IR JSON loader.
 *
 * Walks a cJSON tree shaped like `ui-ir/src/schema.ts` and drives
 * the builder API to assemble the widget tree. Strings referenced by the
 * builder (widget ids, button/label text, action targets, …) are copied
 * into the runtime's string pool so they survive cJSON_Delete.
 */

#include "guimintlab_loader.h"

#include <string.h>
#include <stdlib.h>

#include "cJSON.h"
#include "esp_log.h"

#include "gui/widgets/gui_button.h"

#include "guimintlab_builder.h"
#include "guimintlab_runtime_internal.h"
#include "guimintlab_schema.h"

static const char *TAG = "gml_loader";

/* ---------------------------------------------------------------------------
 * Small helpers.
 * --------------------------------------------------------------------------*/

static const char *json_string(const cJSON *obj, const char *key, const char *dflt) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(v) && v->valuestring != NULL) {
        return v->valuestring;
    }
    return dflt;
}

static int json_int(const cJSON *obj, const char *key, int dflt) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(v)) {
        return (int)v->valuedouble;
    }
    return dflt;
}

static bool json_bool(const cJSON *obj, const char *key, bool dflt) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(v)) {
        return cJSON_IsTrue(v);
    }
    return dflt;
}

/* Duplicate string into runtime strpool. NULL input -> NULL output. */
static const char *dup_str(guimintlab_t *gml, const char *s) {
    if (s == NULL) {
        return NULL;
    }
    return gml_runtime_strpool_put(gml, s);
}

/* ---------------------------------------------------------------------------
 * Colour / palette resolution.
 * --------------------------------------------------------------------------*/

typedef struct {
    char token[16];
    gui_color_t color;
} palette_entry_t;

typedef struct {
    palette_entry_t entries[16];
    uint8_t count;
} palette_t;

static bool parse_hex_color(const char *hex, gui_color_t *out) {
    if (hex == NULL) {
        return false;
    }
    if (hex[0] == '#') {
        hex++;
    }
    size_t n = strlen(hex);
    if (n != 6 && n != 8) {
        return false;
    }
    char buf[9];
    memcpy(buf, hex, n);
    buf[n] = '\0';
    unsigned long v = strtoul(buf, NULL, 16);
    uint8_t r, g, b;
    if (n == 6) {
        r = (v >> 16) & 0xFF;
        g = (v >> 8) & 0xFF;
        b = v & 0xFF;
    } else {
        /* #RRGGBBAA — drop alpha, we don't composite on embedded. */
        r = (v >> 24) & 0xFF;
        g = (v >> 16) & 0xFF;
        b = (v >> 8) & 0xFF;
    }
    *out = gui_color_rgb565(r, g, b);
    return true;
}

static void palette_add(palette_t *p, const char *token, gui_color_t color) {
    if (token == NULL || p->count >= (uint8_t)(sizeof(p->entries) / sizeof(p->entries[0]))) {
        return;
    }
    strncpy(p->entries[p->count].token, token, sizeof(p->entries[p->count].token) - 1);
    p->entries[p->count].token[sizeof(p->entries[p->count].token) - 1] = '\0';
    p->entries[p->count].color = color;
    p->count++;
}

static bool palette_find(const palette_t *p, const char *token, gui_color_t *out) {
    if (token == NULL) {
        return false;
    }
    for (uint8_t i = 0; i < p->count; ++i) {
        if (strcmp(p->entries[i].token, token) == 0) {
            *out = p->entries[i].color;
            return true;
        }
    }
    return false;
}

/* IR colour shape: { "kind": "hex"|"token", "value"/"token": ... }
 * Returns true if colour resolved. */
static bool resolve_color(const cJSON *obj, const palette_t *palette, gui_color_t *out) {
    if (!cJSON_IsObject(obj)) {
        return false;
    }
    const char *kind = json_string(obj, "kind", NULL);
    if (kind == NULL) {
        return false;
    }
    if (strcmp(kind, "hex") == 0) {
        return parse_hex_color(json_string(obj, "value", NULL), out);
    }
    if (strcmp(kind, "token") == 0) {
        return palette_find(palette, json_string(obj, "token", NULL), out);
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Enum mapping.
 * --------------------------------------------------------------------------*/

static gml_layout_mode_t parse_layout_mode(const char *s) {
    if (s == NULL) return GML_LAYOUT_ABSOLUTE;
    if (strcmp(s, "row") == 0) return GML_LAYOUT_ROW;
    if (strcmp(s, "column") == 0) return GML_LAYOUT_COLUMN;
    return GML_LAYOUT_ABSOLUTE;
}

static gml_align_t parse_align(const char *s) {
    if (s == NULL) return GML_ALIGN_START;
    if (strcmp(s, "center") == 0) return GML_ALIGN_CENTER;
    if (strcmp(s, "end") == 0) return GML_ALIGN_END;
    if (strcmp(s, "stretch") == 0) return GML_ALIGN_STRETCH;
    if (strcmp(s, "space-between") == 0) return GML_ALIGN_SPACE_BETWEEN;
    return GML_ALIGN_START;
}

static gml_label_align_t parse_label_align(const char *s) {
    if (s == NULL) return GML_LABEL_ALIGN_LEFT;
    if (strcmp(s, "center") == 0) return GML_LABEL_ALIGN_CENTER;
    if (strcmp(s, "right") == 0) return GML_LABEL_ALIGN_RIGHT;
    return GML_LABEL_ALIGN_LEFT;
}

static const gui_font_t *resolve_font_face(const cJSON *props) {
    if (!cJSON_IsObject(props)) {
        return NULL;
    }

    const char *face_id = json_string(props, "fontFace", NULL);
    if (face_id == NULL) {
        face_id = json_string(props, "fontId", NULL);
    }
    if (face_id == NULL) {
        face_id = json_string(props, "font", NULL);
    }
    if (face_id != NULL) {
        const gui_font_t *font = gui_font_find_by_id(face_id);
        if (font != NULL) {
            return font;
        }
    }

    const char *family = json_string(props, "fontFamily", NULL);
    if (family == NULL) {
        return NULL;
    }

    const int size = json_int(props, "fontSize", 0);
    const gui_font_style_t style = gui_font_parse_style(json_string(props, "fontStyle", "regular"));
    if (size <= 0) {
        return NULL;
    }
    return gui_font_find_face(family, (uint16_t)size, style);
}

static gml_widget_type_t parse_widget_type(const char *s) {
    if (s == NULL) return GML_WIDGET_TYPE_PANEL;
    if (strcmp(s, "screen") == 0) return GML_WIDGET_TYPE_SCREEN;
    if (strcmp(s, "panel") == 0)  return GML_WIDGET_TYPE_PANEL;
    if (strcmp(s, "label") == 0)  return GML_WIDGET_TYPE_LABEL;
    if (strcmp(s, "button") == 0) return GML_WIDGET_TYPE_BUTTON;
    if (strcmp(s, "icon") == 0)   return GML_WIDGET_TYPE_ICON;
    if (strcmp(s, "image") == 0)  return GML_WIDGET_TYPE_IMAGE;
    if (strcmp(s, "line") == 0)   return GML_WIDGET_TYPE_LINE;
    if (strcmp(s, "rect") == 0)   return GML_WIDGET_TYPE_RECT;
    return GML_WIDGET_TYPE_PANEL;
}

static gml_action_kind_t parse_action_kind(const char *s) {
    if (s == NULL) return GML_ACTION_NONE;
    if (strcmp(s, "navigate") == 0) return GML_ACTION_NAVIGATE;
    if (strcmp(s, "show") == 0)     return GML_ACTION_SHOW;
    if (strcmp(s, "hide") == 0)     return GML_ACTION_HIDE;
    if (strcmp(s, "setText") == 0)  return GML_ACTION_SET_TEXT;
    if (strcmp(s, "setValue") == 0) return GML_ACTION_SET_VALUE;
    return GML_ACTION_NONE;
}

/* ---------------------------------------------------------------------------
 * Layout / style application.
 * --------------------------------------------------------------------------*/

static void apply_layout(gml_project_t *project, gml_handle_t h, const cJSON *layout) {
    if (!cJSON_IsObject(layout)) {
        return;
    }
    gml_layout_t l = {
        .mode    = parse_layout_mode(json_string(layout, "mode", "absolute")),
        .padding = (uint8_t)json_int(layout, "padding", 0),
        .gap     = (uint8_t)json_int(layout, "gap", 0),
        .align   = parse_align(json_string(layout, "align", "start")),
        .justify = parse_align(json_string(layout, "justify", "start")),
    };
    gml_project_set_layout(project, h, l);
}

static void apply_style(gml_project_t *project, gml_handle_t h, const cJSON *style, const palette_t *palette) {
    if (!cJSON_IsObject(style)) {
        return;
    }
    gml_style_t s = {0};

    gui_color_t c;
    const cJSON *bg = cJSON_GetObjectItemCaseSensitive(style, "background");
    if (resolve_color(bg, palette, &c)) {
        s.has_background = true;
        s.background = c;
    }
    const cJSON *bc = cJSON_GetObjectItemCaseSensitive(style, "borderColor");
    if (resolve_color(bc, palette, &c)) {
        s.has_border_color = true;
        s.border_color = c;
    }
    const cJSON *tc = cJSON_GetObjectItemCaseSensitive(style, "textColor");
    if (resolve_color(tc, palette, &c)) {
        s.has_text_color = true;
        s.text_color = c;
    }
    const cJSON *bw = cJSON_GetObjectItemCaseSensitive(style, "borderWidth");
    if (cJSON_IsNumber(bw)) {
        s.has_border_width = true;
        s.border_width = (uint8_t)bw->valuedouble;
    }
    const cJSON *dbg = cJSON_GetObjectItemCaseSensitive(style, "drawBackground");
    if (cJSON_IsBool(dbg)) {
        s.has_draw_background = true;
        s.draw_background = cJSON_IsTrue(dbg);
    }
    const cJSON *dbo = cJSON_GetObjectItemCaseSensitive(style, "drawBorder");
    if (cJSON_IsBool(dbo)) {
        s.has_draw_border = true;
        s.draw_border = cJSON_IsTrue(dbo);
    }

    gml_project_set_style(project, h, s);
}

/* ---------------------------------------------------------------------------
 * onPress handling — stores action into press table and wires trampoline.
 * --------------------------------------------------------------------------*/

static void apply_on_press(guimintlab_t *gml,
                           gml_handle_t handle,
                           const char *widget_id,
                           const cJSON *on_press)
{
    if (!cJSON_IsObject(on_press)) {
        return;
    }
    gml_action_kind_t kind = parse_action_kind(json_string(on_press, "kind", NULL));
    if (kind == GML_ACTION_NONE) {
        return;
    }
    const char *target = dup_str(gml, json_string(on_press, "target", NULL));
    const char *value  = dup_str(gml, json_string(on_press, "value", NULL));

    gml_project_set_on_press(&gml->project, handle, kind, target, value);

    /* Allocate a press entry keyed by widget_id so the trampoline can look it up. */
    gml_press_entry_t *entry = gml_runtime_alloc_press_entry(gml, widget_id);
    if (entry == NULL) {
        ESP_LOGW(TAG, "press table full; button '%s' onPress will not fire", widget_id ? widget_id : "?");
        return;
    }
    entry->action_kind   = kind;
    entry->action_target = target;
    entry->action_value  = value;

    /* Wire the button widget's on_click to our trampoline. */
    gui_widget_t *w = gml_project_widget(&gml->project, handle);
    if (w != NULL && gml_project_widget_type(&gml->project, handle) == GML_WIDGET_TYPE_BUTTON) {
        gui_button_set_on_click((gui_button_t *)w, gml_runtime_internal_button_click, entry);
    }
}

/* ---------------------------------------------------------------------------
 * Recursive widget builder.
 * --------------------------------------------------------------------------*/

static gui_rect_t read_frame(const cJSON *node) {
    const cJSON *frame = cJSON_GetObjectItemCaseSensitive(node, "frame");
    gui_rect_t r = { .x = 0, .y = 0, .width = 0, .height = 0 };
    if (cJSON_IsObject(frame)) {
        r.x      = (int16_t)json_int(frame, "x", 0);
        r.y      = (int16_t)json_int(frame, "y", 0);
        r.width  = (int16_t)json_int(frame, "width", 0);
        r.height = (int16_t)json_int(frame, "height", 0);
    }
    return r;
}

static void build_widget(guimintlab_t *gml,
                         gml_handle_t parent,
                         const cJSON *node,
                         const palette_t *palette);

static void apply_children(guimintlab_t *gml,
                           gml_handle_t parent,
                           const cJSON *children,
                           const palette_t *palette)
{
    if (!cJSON_IsArray(children)) {
        return;
    }
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        build_widget(gml, parent, child, palette);
    }
}

static void build_widget(guimintlab_t *gml,
                         gml_handle_t parent,
                         const cJSON *node,
                         const palette_t *palette)
{
    const char *id_raw = json_string(node, "id", NULL);
    if (id_raw == NULL) {
        ESP_LOGW(TAG, "widget without id — skipping");
        return;
    }
    const char *id = dup_str(gml, id_raw);

    gml_widget_type_t type = parse_widget_type(json_string(node, "type", "panel"));
    gui_rect_t frame = read_frame(node);

    gml_handle_t handle = gml_project_add_widget(&gml->project, parent, id, type, frame);
    if (handle == GML_HANDLE_INVALID) {
        ESP_LOGW(TAG, "failed to add widget '%s'", id);
        return;
    }

    if (!json_bool(node, "visible", true)) {
        gml_project_set_visible(&gml->project, handle, false);
    }
    if (!json_bool(node, "enabled", true)) {
        gml_project_set_enabled(&gml->project, handle, false);
    }

    apply_layout(&gml->project, handle, cJSON_GetObjectItemCaseSensitive(node, "layout"));
    apply_style(&gml->project, handle, cJSON_GetObjectItemCaseSensitive(node, "style"), palette);

    const cJSON *props = cJSON_GetObjectItemCaseSensitive(node, "props");
    switch (type) {
    case GML_WIDGET_TYPE_LABEL:
        if (cJSON_IsObject(props)) {
            const char *text = dup_str(gml, json_string(props, "text", NULL));
            if (text != NULL) {
                gml_project_set_label_text(&gml->project, handle, text);
            }
            gml_project_set_label_align(&gml->project, handle,
                                        parse_label_align(json_string(props, "align", "left")));
            const gui_font_t *font = resolve_font_face(props);
            if (font != NULL) {
                gml_project_set_label_font(&gml->project, handle, font);
            }
        }
        break;
    case GML_WIDGET_TYPE_BUTTON:
        if (cJSON_IsObject(props)) {
            const char *text = dup_str(gml, json_string(props, "text", NULL));
            if (text != NULL) {
                gml_project_set_button_text(&gml->project, handle, text);
            }
            uint8_t px = (uint8_t)json_int(props, "paddingX", 0);
            uint8_t py = (uint8_t)json_int(props, "paddingY", 0);
            gml_project_set_button_padding(&gml->project, handle, px, py);
            const gui_font_t *font = resolve_font_face(props);
            if (font != NULL) {
                gml_project_set_button_font(&gml->project, handle, font);
            }

            gui_color_t pressed_bg;
            const cJSON *pb = cJSON_GetObjectItemCaseSensitive(props, "pressedBg");
            if (resolve_color(pb, palette, &pressed_bg)) {
                gml_project_set_button_pressed_bg(&gml->project, handle, pressed_bg);
            }
        }
        apply_on_press(gml, handle, id, cJSON_GetObjectItemCaseSensitive(node, "onPress"));
        break;
    case GML_WIDGET_TYPE_ICON:
        if (cJSON_IsObject(props)) {
            const char *asset = dup_str(
                gml,
                json_string(props, "assetId", json_string(props, "iconId", NULL))
            );
            if (asset != NULL) {
                gml_project_set_icon_asset_id(&gml->project, handle, asset);
            }
            gui_color_t tint;
            const cJSON *c = cJSON_GetObjectItemCaseSensitive(props, "color");
            if (resolve_color(c, palette, &tint)) {
                gml_project_set_icon_color(&gml->project, handle, tint);
            }
        }
        break;
    default:
        break;
    }

    apply_children(gml, handle, cJSON_GetObjectItemCaseSensitive(node, "children"), palette);
}

/* ---------------------------------------------------------------------------
 * Top-level loader.
 * --------------------------------------------------------------------------*/

static void load_palette(const cJSON *project_json, palette_t *palette) {
    palette->count = 0;
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(project_json, "palette");
    if (!cJSON_IsArray(arr)) {
        return;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        const char *token = json_string(item, "token", NULL);
        const char *hex   = json_string(item, "hex", NULL);
        gui_color_t c;
        if (token && hex && parse_hex_color(hex, &c)) {
            palette_add(palette, token, c);
        }
    }
}

esp_err_t gml_loader_load_from_memory(guimintlab_t *gml, const void *json, size_t size)
{
    if (gml == NULL || json == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)json, size);
    if (root == NULL) {
        const char *err = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "JSON parse failed near: %s", err ? err : "?");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t rc = ESP_OK;

    const char *schema = json_string(root, "schemaVersion", NULL);
    if (schema == NULL || strcmp(schema, GML_IR_SCHEMA_VERSION) != 0) {
        ESP_LOGE(TAG, "schema mismatch: got '%s', need '%s'",
                 schema ? schema : "(null)", GML_IR_SCHEMA_VERSION);
        rc = ESP_ERR_INVALID_VERSION;
        goto done;
    }

    palette_t palette;
    load_palette(root, &palette);

    const cJSON *screens = cJSON_GetObjectItemCaseSensitive(root, "screens");
    if (!cJSON_IsArray(screens)) {
        ESP_LOGE(TAG, "IR has no 'screens' array");
        rc = ESP_ERR_INVALID_ARG;
        goto done;
    }

    const cJSON *screen;
    cJSON_ArrayForEach(screen, screens) {
        const char *sid_raw = json_string(screen, "id", NULL);
        if (sid_raw == NULL) {
            ESP_LOGW(TAG, "screen without id — skipping");
            continue;
        }
        const char *sid = dup_str(gml, sid_raw);
        int w = json_int(screen, "width", gml->cfg.display.width);
        int h = json_int(screen, "height", gml->cfg.display.height);

        gml_handle_t sh = gml_project_begin_screen(&gml->project, sid,
                                                   (int16_t)w, (int16_t)h);
        if (sh == GML_HANDLE_INVALID) {
            ESP_LOGW(TAG, "failed to begin screen '%s'", sid);
            continue;
        }
        apply_layout(&gml->project, sh, cJSON_GetObjectItemCaseSensitive(screen, "layout"));
        apply_style(&gml->project, sh, cJSON_GetObjectItemCaseSensitive(screen, "style"), &palette);
        apply_children(gml, sh, cJSON_GetObjectItemCaseSensitive(screen, "children"), &palette);
        gml_project_end_screen(&gml->project);
    }

    const char *initial = json_string(root, "initialScreenId", NULL);
    if (initial != NULL) {
        gml_project_show_screen(&gml->project, initial);
    }

done:
    cJSON_Delete(root);
    return rc;
}
