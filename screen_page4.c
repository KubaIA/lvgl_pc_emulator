#include "screen_page4.h"
#include "ui_common.h"
#include "app.h"
#include "config.h"

#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "cJSON.h"
#include <math.h> 

#include "pictures/img_solar.h"
#include "pictures/img_house.h"
#include "pictures/img_grid.h"
#include "pictures/img_battery.h"

#pragma comment(lib, "winhttp.lib")

/* --- UI --- */
static lv_obj_t* s_scr_page4;

/* --- Labelek --- */
static lv_obj_t* lbl_pv_name;
static lv_obj_t* lbl_pv_value;
static lv_obj_t* lbl_pv_unit;

static lv_obj_t* lbl_soc_name;
static lv_obj_t* lbl_soc_value;
static lv_obj_t* lbl_soc_unit;

static lv_obj_t* lbl_load_name;
static lv_obj_t* lbl_load_value;
static lv_obj_t* lbl_load_unit;

static lv_obj_t* lbl_grid_name;
static lv_obj_t* lbl_grid_value;
static lv_obj_t* lbl_grid_unit;

/* --- PV bar --- */
static lv_obj_t* bar_pv_bg;
static lv_obj_t* bar_pv_fill;

/* --- Akku bar --- */
static lv_obj_t* bar_soc;

/* --- Load bar --- */
static lv_obj_t* bar_load_bg;
static lv_obj_t* bar_load_fill;

/* --- Grid bar --- */
static lv_obj_t* bar_grid_bg;
static lv_obj_t* bar_grid_fill;
static lv_obj_t* bar_grid_zero;

/* --- Ikonok --- */
static lv_obj_t* icon_pv;
static lv_obj_t* icon_soc;
static lv_obj_t* icon_load;
static lv_obj_t* icon_grid;

/* --- Inverter target (configból) --- */
static wchar_t s_inverter_host[128];

/* Helper: safe number read */
static int json_get_number(cJSON* obj, const char* key, double* out)
{
    cJSON* it = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsNumber(it)) return 0;
    *out = it->valuedouble;
    return 1;
}

#define STOPS 5

/* --- PV bar update --- */
static void update_pv_bar(int pv_w)
{
    const int max_w = 6000;
    const lv_coord_t bar_w = 180;
    const lv_coord_t bar_h = 444;

    if (pv_w < 0) pv_w = 0;
    if (pv_w > max_w) pv_w = max_w;

    lv_coord_t fill_h = (lv_coord_t)(((double)pv_w / (double)max_w) * bar_h);
    lv_obj_set_size(bar_pv_fill, bar_w, fill_h);
    lv_obj_set_pos(bar_pv_fill, 0, bar_h - fill_h);

    /* Mindig zöld */
    lv_obj_set_style_bg_color(bar_pv_fill, lv_palette_main(LV_PALETTE_GREEN), 0);
}

/* --- Load bar update --- */
static void update_load_bar(int load_w)
{
    const int max_w = 12000;
    const lv_coord_t bar_w = 180;
    const lv_coord_t bar_h = 444;

    if (load_w < 0) load_w = 0;
    if (load_w > max_w) load_w = max_w;

    lv_coord_t fill_h = (lv_coord_t)(((double)load_w / (double)max_w) * bar_h);
    lv_obj_set_size(bar_load_fill, bar_w, fill_h);
    lv_obj_set_pos(bar_load_fill, 0, bar_h - fill_h);

    /* Mindig piros */
    lv_obj_set_style_bg_color(bar_load_fill, lv_palette_main(LV_PALETTE_RED), 0);
}

/* --- Grid bar update --- */
/*
   0 az alsó ponton van.
   - visszatáplálás (grid_w < 0): zöld, max 6000 W
   - vételezés     (grid_w >= 0): piros, max 12000 W
*/
static void update_grid_bar(int grid_w)
{
    const int export_max_w = 6000;   /* visszatáplálás */
    const int import_max_w = 12000;  /* vételezés */
    const lv_coord_t bar_w = 180;
    const lv_coord_t bar_h = 444;

    /* 0-vonal alul */
    lv_obj_set_pos(bar_grid_zero, 0, bar_h - 2);
    lv_obj_set_size(bar_grid_zero, bar_w, 2);

    if (grid_w < 0) {
        int export_w = -grid_w;
        if (export_w > export_max_w) export_w = export_max_w;

        lv_coord_t fill_h = (lv_coord_t)(((double)export_w / (double)export_max_w) * bar_h);
        lv_obj_set_pos(bar_grid_fill, 0, bar_h - fill_h);
        lv_obj_set_size(bar_grid_fill, bar_w, fill_h);
        lv_obj_set_style_bg_color(bar_grid_fill, lv_palette_main(LV_PALETTE_GREEN), 0);
    }
    else {
        int import_w = grid_w;
        if (import_w > import_max_w) import_w = import_max_w;

        lv_coord_t fill_h = (lv_coord_t)(((double)import_w / (double)import_max_w) * bar_h);
        lv_obj_set_pos(bar_grid_fill, 0, bar_h - fill_h);
        lv_obj_set_size(bar_grid_fill, bar_w, fill_h);
        lv_obj_set_style_bg_color(bar_grid_fill, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

/* --- HTTP GET (Eredeti WinHTTP hívás) --- */
static char* http_get_inverter(void)
{
    HINTERNET hSession = WinHttpOpen(L"LVGL Inverter", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;
    HINTERNET hConnect = WinHttpConnect(hSession, s_inverter_host, INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/solar_api/v1/GetPowerFlowRealtimeData.fcgi", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }
    if (!WinHttpReceiveResponse(hRequest, NULL)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }
    DWORD size = 0, downloaded = 0;
    char* chunk = NULL, * result = NULL;
    size_t total = 0;
    for (;;) {
        if (!WinHttpQueryDataAvailable(hRequest, &size) || size == 0) break;
        chunk = (char*)malloc(size + 1);
        if (!chunk) break;
        if (WinHttpReadData(hRequest, chunk, size, &downloaded)) {
            chunk[downloaded] = '\0';
            char* newbuf = (char*)realloc(result, total + downloaded + 1);
            if (newbuf) {
                result = newbuf;
                memcpy(result + total, chunk, downloaded);
                total += downloaded;
                result[total] = '\0';
            }
        }
        free(chunk);
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return result;
}

/* --- Timer callback --- */
static void update_timer_cb(lv_timer_t* t)
{
    (void)t;
    if (lv_scr_act() != s_scr_page4) return;

    char* json_text = http_get_inverter();
    if (!json_text) {
        lv_label_set_text(lbl_pv_value, "N/A");
        lv_label_set_text(lbl_load_value, "N/A");
        lv_label_set_text(lbl_grid_value, "N/A");
        lv_label_set_text(lbl_soc_value, "N/A");

        update_pv_bar(0);
        update_load_bar(0);
        update_grid_bar(0);
        lv_bar_set_value(bar_soc, 0, LV_ANIM_OFF);
        return;
    }

    cJSON* root = cJSON_Parse(json_text);
    free(json_text);
    if (!root) return;

    cJSON* body = cJSON_GetObjectItem(root, "Body");
    cJSON* data = body ? cJSON_GetObjectItem(body, "Data") : NULL;
    cJSON* site = data ? cJSON_GetObjectItem(data, "Site") : NULL;
    cJSON* invs = data ? cJSON_GetObjectItem(data, "Inverters") : NULL;
    cJSON* inv1 = invs ? cJSON_GetObjectItem(invs, "1") : NULL;

    if (site && inv1) {
        double p_pv = 0, p_load = 0, p_grid = 0, soc = 0;
        char buf[32];

        if (json_get_number(site, "P_PV", &p_pv)) {
            snprintf(buf, sizeof(buf), "%.2f", p_pv / 1000.0);
            lv_label_set_text(lbl_pv_value, buf);
            update_pv_bar((int)p_pv);
        }

        if (json_get_number(site, "P_Load", &p_load)) {
            snprintf(buf, sizeof(buf), "%.2f", fabs(p_load) / 1000.0);
            lv_label_set_text(lbl_load_value, buf);
            update_load_bar((int)fabs(p_load));
        }

        // Grid érték abszolút értékkel ---
        if (json_get_number(site, "P_Grid", &p_grid)) {
            // Mindig abszolút értéket írunk ki (fabs), mert a szín jelzi az irányt
            snprintf(buf, sizeof(buf), "%.2f", fabs(p_grid) / 1000.0);
            lv_label_set_text(lbl_grid_value, buf);
            // A bar frissítéséhez továbbra is az eredeti előjeles értéket használjuk
            update_grid_bar((int)p_grid);
        }

        if (json_get_number(inv1, "SOC", &soc)) {
            lv_label_set_text_fmt(lbl_soc_value, "%d", (int)soc);
            lv_bar_set_value(bar_soc, (int)soc, LV_ANIM_ON);
            lv_obj_set_style_bg_color(bar_soc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
        }
    }

    cJSON_Delete(root);
}

/* --- Screen create --- */
lv_obj_t* screen_page4_create(void)
{
    lv_obj_t* scr = lv_obj_create(NULL);
    s_scr_page4 = scr;
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    MultiByteToWideChar(CP_UTF8, 0, g_cfg.inverter.ip, -1, s_inverter_host, 128);

    /* ---- CÍM ---- */
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text_fmt(title, "%s", g_cfg.inverter.ip);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 95, 30);

    /* ---- LABEL STÍLUS ---- */
    static lv_style_t style_name;
    static lv_style_t style_value;
    static lv_style_t style_unit;
    static bool style_inited = false;

    if (!style_inited) {
        style_inited = true;

        lv_style_init(&style_name);
        lv_style_set_text_font(&style_name, &lv_font_montserrat_24);
        lv_style_set_text_color(&style_name, lv_color_white());

        lv_style_init(&style_value);
        lv_style_set_text_font(&style_value, &lv_font_montserrat_48);
        lv_style_set_text_color(&style_value, lv_color_white());

        lv_style_init(&style_unit);
        lv_style_set_text_font(&style_unit, &lv_font_montserrat_32);
        lv_style_set_text_color(&style_unit, lv_color_white());
    }

    const lv_coord_t bar_w = 180;
    const lv_coord_t bar_h = 444;
    const lv_coord_t spacing = 16;
    const lv_coord_t top_y = 16;

    lv_coord_t x_pv = 16;
    lv_coord_t x_soc = x_pv + bar_w + spacing;
    lv_coord_t x_load = x_soc + bar_w + spacing;
    lv_coord_t x_grid = x_load + bar_w + spacing;

    /* PV Bar */
    bar_pv_bg = lv_obj_create(scr);
    lv_obj_set_size(bar_pv_bg, bar_w, bar_h);
    lv_obj_set_pos(bar_pv_bg, x_pv, top_y);
    lv_obj_set_style_bg_color(bar_pv_bg, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(bar_pv_bg, LV_OPA_30, 0);
    lv_obj_set_style_border_width(bar_pv_bg, 0, 0);
    lv_obj_set_style_radius(bar_pv_bg, 10, 0);
    lv_obj_set_style_pad_all(bar_pv_bg, 0, 0);
    lv_obj_clear_flag(bar_pv_bg, LV_OBJ_FLAG_SCROLLABLE);

    bar_pv_fill = lv_obj_create(bar_pv_bg);
    lv_obj_set_style_border_width(bar_pv_fill, 0, 0);
    lv_obj_set_style_radius(bar_pv_fill, 10, 0);
    lv_obj_clear_flag(bar_pv_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar_pv_fill, lv_palette_main(LV_PALETTE_GREEN), 0);

    /* Akku Bar */
    bar_soc = lv_bar_create(scr);
    lv_obj_set_size(bar_soc, bar_w, bar_h);
    lv_obj_set_pos(bar_soc, x_soc, top_y);
    lv_obj_set_style_bg_color(bar_soc, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_soc, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_soc, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_soc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_soc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);

    /* Load Bar */
    bar_load_bg = lv_obj_create(scr);
    lv_obj_set_size(bar_load_bg, bar_w, bar_h);
    lv_obj_set_pos(bar_load_bg, x_load, top_y);
    lv_obj_set_style_bg_color(bar_load_bg, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(bar_load_bg, LV_OPA_30, 0);
    lv_obj_set_style_border_width(bar_load_bg, 0, 0);
    lv_obj_set_style_radius(bar_load_bg, 10, 0);
    lv_obj_set_style_pad_all(bar_load_bg, 0, 0);
    lv_obj_clear_flag(bar_load_bg, LV_OBJ_FLAG_SCROLLABLE);

    bar_load_fill = lv_obj_create(bar_load_bg);
    lv_obj_set_style_border_width(bar_load_fill, 0, 0);
    lv_obj_set_style_radius(bar_load_fill, 10, 0);
    lv_obj_clear_flag(bar_load_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar_load_fill, lv_palette_main(LV_PALETTE_RED), 0);

    /* Grid Bar */
    bar_grid_bg = lv_obj_create(scr);
    lv_obj_set_size(bar_grid_bg, bar_w, bar_h);
    lv_obj_set_pos(bar_grid_bg, x_grid, top_y);
    lv_obj_set_style_bg_color(bar_grid_bg, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(bar_grid_bg, LV_OPA_30, 0);
    lv_obj_set_style_border_width(bar_grid_bg, 0, 0);
    lv_obj_set_style_radius(bar_grid_bg, 10, 0);
    lv_obj_set_style_pad_all(bar_grid_bg, 0, 0);
    lv_obj_clear_flag(bar_grid_bg, LV_OBJ_FLAG_SCROLLABLE);

    bar_grid_fill = lv_obj_create(bar_grid_bg);
    lv_obj_set_style_border_width(bar_grid_fill, 0, 0);
    lv_obj_set_style_radius(bar_grid_fill, 10, 0);
    lv_obj_clear_flag(bar_grid_fill, LV_OBJ_FLAG_SCROLLABLE);

    bar_grid_zero = lv_obj_create(bar_grid_bg);
    // Itt tesszük láthatatlanná
    lv_obj_set_style_bg_opa(bar_grid_zero, LV_OPA_0, 0);  // LV_OPA_0 = átlátszóság, LV_OPA_100 = teljesen látható
    lv_obj_set_style_border_width(bar_grid_zero, 0, 0);

    /* Ikonok - teljesen javított változat */
    icon_pv = lv_img_create(scr);
    lv_img_set_src(icon_pv, &img_solar);
    lv_obj_set_style_image_recolor(icon_pv, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_pv, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(icon_pv, lv_color_white(), LV_STATE_DEFAULT); // Biztonsági

    icon_soc = lv_img_create(scr);
    lv_img_set_src(icon_soc, &img_battery);
    lv_obj_set_style_image_recolor(icon_soc, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_soc, LV_OPA_COVER, 0);

    icon_load = lv_img_create(scr);
    lv_img_set_src(icon_load, &img_house);
    lv_obj_set_style_image_recolor(icon_load, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_load, LV_OPA_COVER, 0);

    icon_grid = lv_img_create(scr);
    lv_img_set_src(icon_grid, &img_grid);
    lv_obj_set_style_image_recolor(icon_grid, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_grid, LV_OPA_COVER, 0);

    /* Labelek */
    lbl_pv_name = lv_label_create(scr);   lbl_pv_value = lv_label_create(scr);   lbl_pv_unit = lv_label_create(scr);
    lbl_soc_name = lv_label_create(scr);  lbl_soc_value = lv_label_create(scr);  lbl_soc_unit = lv_label_create(scr);
    lbl_load_name = lv_label_create(scr); lbl_load_value = lv_label_create(scr); lbl_load_unit = lv_label_create(scr);
    lbl_grid_name = lv_label_create(scr); lbl_grid_value = lv_label_create(scr); lbl_grid_unit = lv_label_create(scr);

    /* Style-ok */
    lv_obj_add_style(lbl_pv_name, &style_name, 0);
    lv_obj_add_style(lbl_soc_name, &style_name, 0);
    lv_obj_add_style(lbl_load_name, &style_name, 0);
    lv_obj_add_style(lbl_grid_name, &style_name, 0);

    lv_obj_add_style(lbl_pv_value, &style_value, 0);
    lv_obj_add_style(lbl_soc_value, &style_value, 0);
    lv_obj_add_style(lbl_load_value, &style_value, 0);
    lv_obj_add_style(lbl_grid_value, &style_value, 0);

    lv_obj_add_style(lbl_pv_unit, &style_unit, 0);
    lv_obj_add_style(lbl_soc_unit, &style_unit, 0);
    lv_obj_add_style(lbl_load_unit, &style_unit, 0);
    lv_obj_add_style(lbl_grid_unit, &style_unit, 0);

    /* Value mezők fix szélességgel, hogy tényleg középen maradjanak */
    lv_obj_set_width(lbl_pv_value, 120);
    lv_obj_set_width(lbl_soc_value, 120);
    lv_obj_set_width(lbl_load_value, 120);
    lv_obj_set_width(lbl_grid_value, 120);

    lv_obj_set_style_text_align(lbl_pv_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(lbl_soc_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(lbl_load_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(lbl_grid_value, LV_TEXT_ALIGN_CENTER, 0);

    /* Szövegek */
    lv_label_set_text(lbl_pv_name, "Solar");
    lv_label_set_text(lbl_soc_name, "Battery");
    lv_label_set_text(lbl_load_name, "House");
    lv_label_set_text(lbl_grid_name, "Grid");

    lv_label_set_text(lbl_pv_unit, "kW");
    lv_label_set_text(lbl_soc_unit, "%");
    lv_label_set_text(lbl_load_unit, "kW");
    lv_label_set_text(lbl_grid_unit, "kW");

    /* Közös függőleges eltolás */
    const lv_coord_t y_shift = 90;
    const lv_coord_t icon_y = -150 + y_shift;

    /* Ikonok */
    lv_obj_align_to(icon_pv, bar_pv_bg, LV_ALIGN_CENTER, 0, icon_y);
    lv_obj_align_to(icon_soc, bar_soc, LV_ALIGN_CENTER, 0, icon_y);
    lv_obj_align_to(icon_load, bar_load_bg, LV_ALIGN_CENTER, 0, icon_y);
    lv_obj_align_to(icon_grid, bar_grid_bg, LV_ALIGN_CENTER, 0, icon_y);

    /* PV */
    lv_obj_align_to(lbl_pv_name, bar_pv_bg, LV_ALIGN_CENTER, 0, -40 + y_shift);
    lv_obj_align_to(lbl_pv_value, bar_pv_bg, LV_ALIGN_CENTER, 0, 0 + y_shift);
    lv_obj_align_to(lbl_pv_unit, bar_pv_bg, LV_ALIGN_CENTER, 0, 40 + y_shift);

    /* Battery */
    lv_obj_align_to(lbl_soc_name, bar_soc, LV_ALIGN_CENTER, 0, -40 + y_shift);
    lv_obj_align_to(lbl_soc_value, bar_soc, LV_ALIGN_CENTER, 0, 0 + y_shift);
    lv_obj_align_to(lbl_soc_unit, bar_soc, LV_ALIGN_CENTER, 0, 40 + y_shift);

    /* House */
    lv_obj_align_to(lbl_load_name, bar_load_bg, LV_ALIGN_CENTER, 0, -40 + y_shift);
    lv_obj_align_to(lbl_load_value, bar_load_bg, LV_ALIGN_CENTER, 0, 0 + y_shift);
    lv_obj_align_to(lbl_load_unit, bar_load_bg, LV_ALIGN_CENTER, 0, 40 + y_shift);

    /* Grid */
    lv_obj_align_to(lbl_grid_name, bar_grid_bg, LV_ALIGN_CENTER, 0, -40 + y_shift);
    lv_obj_align_to(lbl_grid_value, bar_grid_bg, LV_ALIGN_CENTER, 0, 0 + y_shift);
    lv_obj_align_to(lbl_grid_unit, bar_grid_bg, LV_ALIGN_CENTER, 0, 40 + y_shift);

    update_pv_bar(0);
    update_load_bar(0);
    update_grid_bar(0);

    uint32_t period = g_cfg.inverter.refresh_ms;
    if (period < 500) period = 500;
    lv_timer_create(update_timer_cb, period, NULL);

    return scr;
}