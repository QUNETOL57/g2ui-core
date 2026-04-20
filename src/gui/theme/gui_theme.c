#include "gui/theme/gui_theme.h"

#include "gui/generated/gui_assets_generated.h"

void gui_theme_init_default(gui_theme_t *theme)
{
    theme->screen_bg = gui_color_rgb565(4, 4, 4);
    theme->screen_grid = gui_color_rgb565(20, 20, 20);
    theme->surface_base = gui_color_rgb565(10, 10, 10);
    theme->surface_raised = gui_color_rgb565(18, 18, 18);
    theme->surface_inverse = gui_color_rgb565(232, 232, 232);
    theme->line_soft = gui_color_rgb565(34, 34, 34);
    theme->line_strong = gui_color_rgb565(92, 92, 92);
    theme->accent = gui_color_rgb565(255, 255, 255);
    theme->accent_soft = gui_color_rgb565(168, 168, 168);
    theme->text_inverse = gui_color_rgb565(0, 0, 0);
    theme->danger = gui_color_rgb565(160, 160, 160);
    theme->white = gui_color_rgb565(255, 255, 255);
    theme->black = gui_color_rgb565(0, 0, 0);
    theme->text_primary = gui_color_rgb565(255, 255, 255);
    theme->text_muted = gui_color_rgb565(132, 132, 132);
    theme->border_thin = 1;
    theme->border_heavy = 2;
    theme->space_xs = 2;
    theme->space_sm = 4;
    theme->space_md = 8;
    theme->space_lg = 12;
    theme->title_scale = 2;
    theme->value_scale = 2;
    theme->button_scale = 1;
    theme->font_small = &gui_font_default_5x7;
    theme->font_body = &gui_font_default_5x7;
}
