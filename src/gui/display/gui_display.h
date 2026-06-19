#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "hal/spi_types.h"

#include "gui/core/gui_types.h"

typedef struct {
    spi_host_device_t host;
    int pin_mosi;
    int pin_sclk;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
    bool backlight_on_level;
    int width;
    int height;
    int spi_clock_hz;
    size_t transfer_rows;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    int gap_x;          /* column offset in ST7789 internal memory */
    int gap_y;          /* row offset in ST7789 internal memory */
    bool pre_invert;    /* pre-invert pixel data for displays with INVON hardwired */
} gui_display_config_t;

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    gui_display_config_t config;
    bool backlight_ready;
    gui_color_t *tx_chunk_buffer;
    void *color_done_sem;
} gui_display_t;

esp_err_t gui_display_init(gui_display_t *display, const gui_display_config_t *config);
esp_err_t gui_display_flush_rect(gui_display_t *display, gui_rect_t rect, const gui_color_t *pixels, int source_stride);
esp_err_t gui_display_fill_rect(gui_display_t *display, gui_rect_t rect, const gui_color_t *row_pixels);
void gui_display_set_backlight(gui_display_t *display, bool enabled);
gui_rect_t gui_display_bounds(const gui_display_t *display);
