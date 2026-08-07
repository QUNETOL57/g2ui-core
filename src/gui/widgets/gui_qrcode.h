#pragma once

#include <stdbool.h>

#include "gui/core/gui_widget.h"

typedef enum {
    GUI_QRCODE_ECC_LOW = 0,
    GUI_QRCODE_ECC_MEDIUM,
    GUI_QRCODE_ECC_QUARTILE,
    GUI_QRCODE_ECC_HIGH,
} gui_qrcode_ecc_t;

typedef enum {
    GUI_QRCODE_SIZE_XXS = 1,
    GUI_QRCODE_SIZE_XS = 2,
    GUI_QRCODE_SIZE_S = 3,
    GUI_QRCODE_SIZE_M = 4,
    GUI_QRCODE_SIZE_L = 5,
    GUI_QRCODE_SIZE_XL = 6,
    GUI_QRCODE_SIZE_XXL = 7,
} gui_qrcode_size_t;

typedef struct {
    gui_widget_t base;
    const char *text;
    uint8_t *qrcode;
    uint16_t qrcode_capacity;
    uint8_t version;
    uint8_t module_scale;
    uint8_t module_count;
    gui_qrcode_ecc_t ecc;
    gui_color_t foreground;
    gui_color_t background;
    gui_color_t border_color;
    uint8_t border_width;
    bool draw_background;
    bool draw_border;
    bool encoded;
} gui_qrcode_t;

void gui_qrcode_init(gui_qrcode_t *qrcode);
uint16_t gui_qrcode_buffer_len_for_version(uint8_t version);
void gui_qrcode_set_buffer(gui_qrcode_t *qrcode, uint8_t *buffer, uint16_t capacity);
void gui_qrcode_set_config(gui_qrcode_t *qrcode,
                           const char *text,
                           uint8_t version,
                           gui_qrcode_ecc_t ecc,
                           gui_qrcode_size_t size);
void gui_qrcode_set_style(gui_qrcode_t *qrcode,
                          gui_color_t foreground,
                          gui_color_t background,
                          bool draw_background,
                          gui_color_t border_color,
                          uint8_t border_width,
                          bool draw_border);
