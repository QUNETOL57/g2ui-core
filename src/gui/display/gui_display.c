#include "gui/display/gui_display.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static esp_err_t gui_display_configure_backlight(gui_display_t *display)
{
    if (display->config.pin_bl < 0) {
        display->backlight_ready = false;
        return ESP_OK;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << display->config.pin_bl,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), "gui_display", "configure backlight");
    display->backlight_ready = true;
    gui_display_set_backlight(display, false);
    return ESP_OK;
}

static inline gui_color_t gui_display_swap_color_bytes(gui_color_t color)
{
    return (gui_color_t)((color >> 8) | (color << 8));
}

static bool gui_display_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;

    gui_display_t *display = (gui_display_t *)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;
    SemaphoreHandle_t color_done_sem = display != NULL ? (SemaphoreHandle_t)display->color_done_sem : NULL;
    if (color_done_sem != NULL) {
        xSemaphoreGiveFromISR(color_done_sem, &high_task_wakeup);
    }
    return high_task_wakeup == pdTRUE;
}

static esp_err_t gui_display_wait_color_done(gui_display_t *display)
{
    SemaphoreHandle_t color_done_sem = display != NULL ? (SemaphoreHandle_t)display->color_done_sem : NULL;
    if (color_done_sem == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return xSemaphoreTake(color_done_sem, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t gui_display_prepare_tx_chunk(gui_display_t *display, const gui_color_t *src, int width, int height, int source_stride)
{
    if (display == NULL || src == NULL || width <= 0 || height <= 0 || source_stride < width || display->tx_chunk_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int row = 0; row < height; row++) {
        const gui_color_t *src_row = src + (size_t)row * source_stride;
        gui_color_t *dst_row = display->tx_chunk_buffer + (size_t)row * width;
        for (int col = 0; col < width; col++) {
            dst_row[col] = gui_display_swap_color_bytes(src_row[col]);
        }
    }

    return ESP_OK;
}

static esp_err_t gui_display_prepare_fill_chunk(gui_display_t *display, const gui_color_t *src_row, int width, int height)
{
    if (display == NULL || src_row == NULL || width <= 0 || height <= 0 || display->tx_chunk_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int row = 0; row < height; row++) {
        gui_color_t *dst_row = display->tx_chunk_buffer + (size_t)row * width;
        for (int col = 0; col < width; col++) {
            dst_row[col] = gui_display_swap_color_bytes(src_row[col]);
        }
    }

    return ESP_OK;
}

esp_err_t gui_display_init(gui_display_t *display, const gui_display_config_t *config)
{
    if (display == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(display, 0, sizeof(*display));
    display->config = *config;

    ESP_RETURN_ON_ERROR(gui_display_configure_backlight(display), "gui_display", "backlight init failed");

    spi_bus_config_t buscfg = {
        .mosi_io_num = config->pin_mosi,
        .miso_io_num = -1,
        .sclk_io_num = config->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->width * config->transfer_rows * sizeof(gui_color_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(config->host, &buscfg, SPI_DMA_CH_AUTO), "gui_display", "spi init failed");

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = config->pin_dc,
        .cs_gpio_num = config->pin_cs,
        .pclk_hz = config->spi_clock_hz,
        .on_color_trans_done = gui_display_on_color_trans_done,
        .user_ctx = display,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config->host, &io_cfg, &display->io),
        "gui_display",
        "panel io init failed");

    display->color_done_sem = (void *)xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(display->color_done_sem != NULL, ESP_ERR_NO_MEM, "gui_display", "color done semaphore alloc failed");

    const size_t chunk_rows = config->transfer_rows > 0 ? config->transfer_rows : 1;
    const size_t chunk_pixels = (size_t)config->width * chunk_rows;
    display->tx_chunk_buffer = heap_caps_malloc(chunk_pixels * sizeof(gui_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (display->tx_chunk_buffer == NULL) {
        display->tx_chunk_buffer = heap_caps_malloc(chunk_pixels * sizeof(gui_color_t), MALLOC_CAP_DEFAULT);
    }
    ESP_RETURN_ON_FALSE(display->tx_chunk_buffer != NULL, ESP_ERR_NO_MEM, "gui_display", "tx chunk buffer alloc failed");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = config->pin_rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(display->io, &panel_cfg, &display->panel), "gui_display", "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(display->panel), "gui_display", "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(display->panel), "gui_display", "panel basic init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(display->panel, config->swap_xy), "gui_display", "panel swap_xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(display->panel, config->mirror_x, config->mirror_y), "gui_display", "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(display->panel, true), "gui_display", "panel enable failed");

    gui_display_set_backlight(display, true);
    return ESP_OK;
}

gui_rect_t gui_display_bounds(const gui_display_t *display)
{
    return gui_rect_make(0, 0, display->config.width, display->config.height);
}

void gui_display_set_backlight(gui_display_t *display, bool enabled)
{
    if (display == NULL || !display->backlight_ready) {
        return;
    }

    const int level = enabled == display->config.backlight_on_level ? 1 : 0;
    gpio_set_level(display->config.pin_bl, level);
}

esp_err_t gui_display_flush_rect(gui_display_t *display, gui_rect_t rect, const gui_color_t *pixels, int source_stride)
{
    if (display == NULL || pixels == NULL || gui_rect_is_empty(rect)) {
        return ESP_ERR_INVALID_ARG;
    }

    const gui_rect_t clipped = gui_rect_intersect(rect, gui_display_bounds(display));
    if (gui_rect_is_empty(clipped)) {
        return ESP_OK;
    }

    const int stride = source_stride > 0 ? source_stride : rect.width;
    const int src_x = clipped.x - rect.x;
    const int src_y = clipped.y - rect.y;
    const int max_chunk_rows = (int)(display->config.transfer_rows > 0 ? display->config.transfer_rows : 1);

    for (int row = 0; row < clipped.height; row += max_chunk_rows) {
        const int chunk_height = (clipped.height - row) < max_chunk_rows ? (clipped.height - row) : max_chunk_rows;
        const gui_color_t *src = pixels + (size_t)(src_y + row) * stride + src_x;
        ESP_RETURN_ON_ERROR(
            gui_display_prepare_tx_chunk(display, src, clipped.width, chunk_height, stride),
            "gui_display",
            "prepare tx chunk failed");
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_draw_bitmap(
                display->panel,
                clipped.x,
                clipped.y + row,
                clipped.x + clipped.width,
                clipped.y + row + chunk_height,
                display->tx_chunk_buffer),
            "gui_display",
            "panel draw failed");
        ESP_RETURN_ON_ERROR(gui_display_wait_color_done(display), "gui_display", "panel draw wait failed");
    }

    return ESP_OK;
}

esp_err_t gui_display_fill_rect(gui_display_t *display, gui_rect_t rect, const gui_color_t *row_pixels)
{
    if (display == NULL || row_pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const gui_rect_t clipped = gui_rect_intersect(rect, gui_display_bounds(display));
    if (gui_rect_is_empty(clipped)) {
        return ESP_OK;
    }

    const int max_chunk_rows = (int)(display->config.transfer_rows > 0 ? display->config.transfer_rows : 1);

    for (int row = 0; row < clipped.height; row += max_chunk_rows) {
        const int chunk_height = (clipped.height - row) < max_chunk_rows ? (clipped.height - row) : max_chunk_rows;
        ESP_RETURN_ON_ERROR(
            gui_display_prepare_fill_chunk(display, row_pixels, clipped.width, chunk_height),
            "gui_display",
            "prepare fill chunk failed");
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_draw_bitmap(
                display->panel,
                clipped.x,
                clipped.y + row,
                clipped.x + clipped.width,
                clipped.y + row + chunk_height,
                display->tx_chunk_buffer),
            "gui_display",
            "panel fill failed");
        ESP_RETURN_ON_ERROR(gui_display_wait_color_done(display), "gui_display", "panel fill wait failed");
    }

    return ESP_OK;
}
