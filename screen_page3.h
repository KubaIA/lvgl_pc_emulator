#pragma once
#include "lvgl/lvgl.h"

lv_obj_t * screen_page3_create(void);
void screen_page3_force_update_location(void);
bool geocode_city(const char* city_name, double* out_lat, double* out_lon, char* out_display_name, size_t name_sz);