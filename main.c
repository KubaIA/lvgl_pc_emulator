#include <windows.h>
#include "lvgl/lvgl.h"
#include "lv_windows_display.h"
#include "lv_windows_input.h"
#include "app.h"
#include "config.h"
extern void screen_page3_force_update_location(void);

int main(void)
{
    config_set_defaults(&g_cfg);
    config_load_json(&g_cfg, "config.json");

    lv_init();

    lv_display_t* disp = lv_windows_create_display(
        L"LVGL",
        800,
        480,
        40,
        true,
        true
    );

    if (!disp) return -1;
    lv_display_set_default(disp);

    lv_indev_t* mouse = lv_windows_acquire_pointer_indev(disp);
    (void)mouse;

    app_init();

    screen_page3_force_update_location();

    while (1) {
        uint32_t t = lv_timer_handler();
        if (t == 0) t = 1;
        lv_delay_ms(t);
    }
}