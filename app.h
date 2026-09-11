#pragma once
#include "lvgl/lvgl.h"

/* 1–6: page1..page6 (1 = Home, 6 = Settings) */

void app_init(void);

/* Aktuális oldal betöltése (destroy + create) */
void app_load_page(int id);

/* Jelenlegi oldal lekérdezése (ha kell késõbb) */
int app_get_current_page_id(void);
