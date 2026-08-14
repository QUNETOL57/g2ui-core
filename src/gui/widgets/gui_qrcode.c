#include "gui/widgets/gui_qrcode.h"

#include <string.h>

#include "gui/render/gui_renderer.h"
#include "third_party/qrcodegen/qrcodegen.h"

static uint8_t clamp_version(uint8_t version)
{
    if (version < qrcodegen_VERSION_MIN) {
        return qrcodegen_VERSION_MIN;
    }
    if (version > qrcodegen_VERSION_MAX) {
        return qrcodegen_VERSION_MAX;
    }
    return version;
}

static enum qrcodegen_Ecc to_qrcodegen_ecc(gui_qrcode_ecc_t ecc)
{
    switch (ecc) {
        case GUI_QRCODE_ECC_LOW:
            return qrcodegen_Ecc_LOW;
        case GUI_QRCODE_ECC_QUARTILE:
            return qrcodegen_Ecc_QUARTILE;
        case GUI_QRCODE_ECC_HIGH:
            return qrcodegen_Ecc_HIGH;
        case GUI_QRCODE_ECC_MEDIUM:
        default:
            return qrcodegen_Ecc_MEDIUM;
    }
}

static void encode(gui_qrcode_t *qrcode)
{
    if (qrcode == NULL) {
        return;
    }
    qrcode->encoded = false;
    qrcode->module_count = 0;
    if (qrcode->qrcode == NULL || qrcode->text == NULL) {
        return;
    }

    const uint8_t version = clamp_version(qrcode->version);
    const uint16_t required = gui_qrcode_buffer_len_for_version(version);
    if (qrcode->qrcode_capacity < required) {
        return;
    }

    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    const size_t text_len = strlen(qrcode->text);
    if (text_len > sizeof(temp)) {
        return;
    }
    memcpy(temp, qrcode->text, text_len);

    qrcode->encoded = qrcodegen_encodeBinary(temp,
                                             text_len,
                                             qrcode->qrcode,
                                             to_qrcodegen_ecc(qrcode->ecc),
                                             version,
                                             version,
                                             qrcodegen_Mask_AUTO,
                                             false);
    if (qrcode->encoded) {
        qrcode->module_count = (uint8_t)qrcodegen_getSize(qrcode->qrcode);
    }
}

static int qr_border_inset(const gui_qrcode_t *qrcode)
{
    return (qrcode->draw_border && qrcode->border_width > 0) ? qrcode->border_width : 0;
}

static gui_size_t gui_qrcode_measure(gui_widget_t *widget, gui_size_t available)
{
    gui_qrcode_t *qrcode = (gui_qrcode_t *)widget;
    const int modules = qrcode->module_count > 0 ? qrcode->module_count : (21 + 4 * (clamp_version(qrcode->version) - 1));
    const int rendered = modules * (qrcode->module_scale > 0 ? qrcode->module_scale : 1);
    const int total = rendered + qr_border_inset(qrcode) * 2;
    gui_size_t size = {
        .width = widget->frame.width > 0 ? widget->frame.width : (int16_t)total,
        .height = widget->frame.height > 0 ? widget->frame.height : (int16_t)total,
    };
    if (size.width <= 0) {
        size.width = available.width;
    }
    if (size.height <= 0) {
        size.height = available.height;
    }
    return size;
}

static void gui_qrcode_layout(gui_widget_t *widget)
{
    (void)widget;
}

static void gui_qrcode_paint(gui_widget_t *widget, gui_renderer_t *renderer, gui_rect_t clip_rect)
{
    gui_qrcode_t *qrcode = (gui_qrcode_t *)widget;
    const gui_rect_t absolute = gui_widget_absolute_rect(widget);
    if (gui_rect_is_empty(gui_rect_intersect(absolute, clip_rect))) {
        return;
    }

    const int module_scale = qrcode->module_scale > 0 ? qrcode->module_scale : 1;
    const int modules = qrcode->module_count > 0 ? qrcode->module_count : (21 + 4 * (clamp_version(qrcode->version) - 1));
    const int rendered = modules * module_scale;
    const int border_width = qr_border_inset(qrcode);
    const bool border_inside = border_width > 0 &&
                               absolute.width >= rendered + border_width * 2 &&
                               absolute.height >= rendered + border_width * 2;
    const int inset = border_inside ? border_width : 0;
    const gui_rect_t rendered_rect = gui_rect_make(absolute.x + inset, absolute.y + inset, rendered, rendered);

    const int normalized_rotation = ((widget->rotation_degrees % 360) + 360) % 360;
    const bool rotation_enabled = (normalized_rotation % 90) == 0 && normalized_rotation != 0;
    if (rotation_enabled) {
        gui_renderer_push_rotation(
            renderer,
            (gui_renderer_rotation_t)(normalized_rotation / 90),
            (gui_point_t){
                .x = (int16_t)(absolute.x + absolute.width / 2),
                .y = (int16_t)(absolute.y + absolute.height / 2),
            });
    }

    if (qrcode->draw_background) {
        gui_renderer_fill_rect(renderer, rendered_rect, qrcode->background);
    }

    if (qrcode->encoded) {
        for (int y = 0; y < qrcode->module_count; y++) {
            for (int x = 0; x < qrcode->module_count; x++) {
                if (!qrcodegen_getModule(qrcode->qrcode, x, y)) {
                    continue;
                }
                gui_renderer_fill_rect(
                    renderer,
                    gui_rect_make(rendered_rect.x + x * module_scale,
                                  rendered_rect.y + y * module_scale,
                                  module_scale,
                                  module_scale),
                    qrcode->foreground);
            }
        }
    }

    if (border_width > 0) {
        const gui_rect_t border_rect = border_inside
            ? absolute
            : gui_rect_make((int16_t)(rendered_rect.x - border_width),
                            (int16_t)(rendered_rect.y - border_width),
                            (int16_t)(rendered_rect.width + border_width * 2),
                            (int16_t)(rendered_rect.height + border_width * 2));
        gui_renderer_stroke_rect(renderer, border_rect, border_width, qrcode->border_color);
    }

    if (rotation_enabled) {
        gui_renderer_pop_rotation(renderer);
    }
}

static bool gui_qrcode_input(gui_widget_t *widget, const gui_input_event_t *event)
{
    (void)widget;
    (void)event;
    return false;
}

static const gui_widget_vtable_t GUI_QRCODE_VTABLE = {
    .measure = gui_qrcode_measure,
    .layout = gui_qrcode_layout,
    .paint = gui_qrcode_paint,
    .input = gui_qrcode_input,
};

void gui_qrcode_init(gui_qrcode_t *qrcode)
{
    gui_widget_init(&qrcode->base, &GUI_QRCODE_VTABLE);
    qrcode->text = "";
    qrcode->version = 1;
    qrcode->module_scale = GUI_QRCODE_SIZE_M;
    qrcode->ecc = GUI_QRCODE_ECC_MEDIUM;
    qrcode->foreground = 0xFFFFu;
    qrcode->background = 0xFFFFu;
    qrcode->border_color = 0xFFFFu;
    qrcode->border_width = 1;
    qrcode->draw_background = false;
    qrcode->draw_border = false;
}

uint16_t gui_qrcode_buffer_len_for_version(uint8_t version)
{
    return (uint16_t)qrcodegen_BUFFER_LEN_FOR_VERSION(clamp_version(version));
}

void gui_qrcode_set_buffer(gui_qrcode_t *qrcode, uint8_t *buffer, uint16_t capacity)
{
    if (qrcode == NULL) {
        return;
    }
    qrcode->qrcode = buffer;
    qrcode->qrcode_capacity = capacity;
    encode(qrcode);
    gui_widget_invalidate(&qrcode->base);
}

void gui_qrcode_set_config(gui_qrcode_t *qrcode,
                           const char *text,
                           uint8_t version,
                           gui_qrcode_ecc_t ecc,
                           gui_qrcode_size_t size)
{
    if (qrcode == NULL) {
        return;
    }
    qrcode->text = text != NULL ? text : "";
    qrcode->version = clamp_version(version);
    qrcode->ecc = ecc;
    qrcode->module_scale = (uint8_t)size;
    if (qrcode->module_scale < GUI_QRCODE_SIZE_XXS || qrcode->module_scale > GUI_QRCODE_SIZE_XXL) {
        qrcode->module_scale = GUI_QRCODE_SIZE_M;
    }
    encode(qrcode);
    gui_widget_invalidate(&qrcode->base);
}

void gui_qrcode_set_style(gui_qrcode_t *qrcode,
                          gui_color_t foreground,
                          gui_color_t background,
                          bool draw_background,
                          gui_color_t border_color,
                          uint8_t border_width,
                          bool draw_border)
{
    if (qrcode == NULL) {
        return;
    }
    qrcode->foreground = foreground;
    qrcode->background = background;
    qrcode->draw_background = draw_background;
    qrcode->border_color = border_color;
    qrcode->border_width = border_width;
    qrcode->draw_border = draw_border && border_width > 0;
    gui_widget_invalidate(&qrcode->base);
}
