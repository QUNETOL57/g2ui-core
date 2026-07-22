#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_log.h"
#include "driver/spi_common.h"

#include "g2ui/g2ui.h"

static const char *TAG = "g2ui_basic";

extern const uint8_t project_json_start[] asm("_binary_project_json_start");
extern const uint8_t project_json_end[] asm("_binary_project_json_end");

void app_main(void)
{
    g2ui_t *gml = NULL;
    const g2ui_config_t cfg = {
        .display = {
            .host = SPI2_HOST,
            .pin_mosi = 11,
            .pin_sclk = 12,
            .pin_cs = 10,
            .pin_dc = 9,
            .pin_rst = 8,
            .pin_bl = 13,
            .backlight_on_level = true,
            .width = 240,
            .height = 320,
            .spi_clock_hz = 40 * 1000 * 1000,
            .transfer_rows = 20,
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
            .gap_x = 0,
            .gap_y = 0,
            .pre_invert = false,
        },
        .max_widgets = 96,
        .max_ids = 96,
        .max_freehand_points = 512,
    };

    ESP_ERROR_CHECK(g2ui_new(&cfg, &gml));
    ESP_ERROR_CHECK(g2ui_load_from_memory(
        gml,
        project_json_start,
        (size_t)(project_json_end - project_json_start)));
    ESP_ERROR_CHECK(g2ui_show_screen(gml, "main"));

    ESP_LOGI(TAG, "G2UI basic example running");
    g2ui_run(gml);
}
