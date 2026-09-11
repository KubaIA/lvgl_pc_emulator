#include "screen_page6.h"
#include "screen_page3.h"
#include "ui_common.h"
#include "app.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Külsõ változók és függvények deklarálása */
extern app_config_t g_cfg;
extern void screen_page3_force_update_location(void);

static lv_obj_t* s_scr_page6;
static lv_obj_t* setting_list;
static lv_obj_t* lbl_current_city_val;

/* Globális pointerek az IP címekhez az átláthatóságért */
static lv_obj_t* lbl_inverter_ip_val;
static lv_obj_t* lbl_ac1_ip_val;
static lv_obj_t* lbl_ac2_ip_val;
static lv_obj_t* lbl_ac3_ip_val;

/* Globális pointerek a Wi-Fi beállításokhoz */
static lv_obj_t* lbl_wifi_ssid_val;
static lv_obj_t* lbl_wifi_password_val;

/* ---- Képernyõ törlésekor pointerek nullázása (Fagyás ellen) ---- */
static void screen_page6_delete_cb(lv_event_t* e) {
    s_scr_page6 = NULL;
    lbl_current_city_val = NULL;
    lbl_inverter_ip_val = NULL;
    lbl_ac1_ip_val = NULL;
    lbl_ac2_ip_val = NULL;
    lbl_ac3_ip_val = NULL;
    lbl_wifi_ssid_val = NULL;
    lbl_wifi_password_val = NULL;
}

/* ---- Segédfüggvény: IPv4 szintaktika ellenõrzése ---- */
static bool validate_ipv4(const char* ip_str) {
    int num1, num2, num3, num4;
    if (sscanf(ip_str, "%d.%d.%d.%d", &num1, &num2, &num3, &num4) == 4) {
        if (num1 >= 0 && num1 <= 255 &&
            num2 >= 0 && num2 <= 255 &&
            num3 >= 0 && num3 <= 255 &&
            num4 >= 0 && num4 <= 255) {
            return true;
        }
    }
    return false;
}

/* ---- Város Popup Mentés Gomb ---- */
static void popup_save_cb(lv_event_t* e) {
    lv_obj_t* popup = (lv_obj_t*)lv_event_get_user_data(e);
    lv_obj_t* ta = lv_obj_get_child(popup, 1);
    const char* text = lv_textarea_get_text(ta);

    if (text && strlen(text) > 0) {
        double lat, lon;
        char display_name[64];

        if (geocode_city(text, &lat, &lon, display_name, sizeof(display_name))) {
            char clean_text[64];

            strncpy(clean_text, display_name, sizeof(clean_text) - 1);
            clean_text[sizeof(clean_text) - 1] = '\0';
            strip_accents(clean_text);

            if (lbl_current_city_val) {
                lv_label_set_text(lbl_current_city_val, clean_text);
            }

            strncpy(g_cfg.weather.city, clean_text, sizeof(g_cfg.weather.city) - 1);
            g_cfg.weather.city[sizeof(g_cfg.weather.city) - 1] = '\0';

            config_save_json(&g_cfg, "config.json");
            screen_page3_force_update_location();
            lv_obj_del_async(popup);
        }
        else {
            lv_obj_set_style_border_color(ta, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_border_width(ta, 3, 0);
        }
    }
    else {
        lv_obj_del_async(popup);
    }
}

/* ---- IP Popup Mentés Gomb ---- */
static void popup_ip_save_cb(lv_event_t* e) {
    char* target_config_str = (char*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* popup = lv_obj_get_parent(btn);
    lv_obj_t* ta = lv_obj_get_child(popup, 1);
    const char* text = lv_textarea_get_text(ta);

    if (text && strlen(text) > 0) {
        if (validate_ipv4(text)) {
            strncpy(target_config_str, text, 16 - 1);
            target_config_str[15] = '\0';

            config_save_json(&g_cfg, "config.json");

            if (target_config_str == g_cfg.inverter.ip && lbl_inverter_ip_val) {
                lv_label_set_text(lbl_inverter_ip_val, text);
            }
            else if (target_config_str == g_cfg.gree.dev[0].ip && lbl_ac1_ip_val) {
                lv_label_set_text(lbl_ac1_ip_val, text);
            }
            else if (target_config_str == g_cfg.gree.dev[1].ip && lbl_ac2_ip_val) {
                lv_label_set_text(lbl_ac2_ip_val, text);
            }
            else if (target_config_str == g_cfg.gree.dev[2].ip && lbl_ac3_ip_val) {
                lv_label_set_text(lbl_ac3_ip_val, text);
            }

            lv_obj_del_async(popup);
        }
        else {
            lv_obj_set_style_border_color(ta, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_border_width(ta, 3, 0);
        }
    }
    else {
        lv_obj_del_async(popup);
    }
}

/* ---- Wi-Fi Popup Mentés Gomb ---- */
static void popup_wifi_save_cb(lv_event_t* e) {
    char* target_config_str = (char*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* popup = lv_obj_get_parent(btn);
    lv_obj_t* ta = lv_obj_get_child(popup, 1);
    const char* text = lv_textarea_get_text(ta);

    if (text && strlen(text) > 0) {
        strncpy(target_config_str, text, 64 - 1);
        target_config_str[63] = '\0';

        config_save_json(&g_cfg, "config.json");

        if (target_config_str == g_cfg.wifi.ssid && lbl_wifi_ssid_val) {
            lv_label_set_text(lbl_wifi_ssid_val, text);
        }
        else if (target_config_str == g_cfg.wifi.password && lbl_wifi_password_val) {
            size_t pass_len = strlen(text);
            char stars[65];
            if (pass_len > 64) pass_len = 64;

            memset(stars, '*', pass_len);
            stars[pass_len] = '\0';
            lv_label_set_text(lbl_wifi_password_val, stars);
        }

        lv_obj_del_async(popup);
    }
    else {
        lv_obj_del_async(popup);
    }
}

static void popup_cancel_cb(lv_event_t* e) {
    lv_obj_t* popup = (lv_obj_t*)lv_event_get_user_data(e);
    lv_obj_del_async(popup);
}

/* ---- Városnév kattintás ---- */
static void city_click_cb(lv_event_t* e) {
    lv_obj_t* popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup, 780, 440);
    lv_obj_center(popup);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(popup, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(popup, 2, 0);

    lv_obj_t* title = lv_label_create(popup);
    lv_label_set_text(title, "Search City");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* ta = lv_textarea_create(popup);
    lv_obj_set_size(ta, 600, 60);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 60);

    if (lbl_current_city_val) {
        lv_textarea_set_text(ta, lv_label_get_text(lbl_current_city_val));
    }

    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x252525), 0);
    lv_obj_set_style_text_color(ta, lv_color_white(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);

    lv_obj_t* kb = lv_keyboard_create(popup);
    lv_obj_set_size(kb, 740, 210);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -65);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_24, LV_PART_ITEMS);

    lv_obj_t* btn_save = lv_btn_create(popup);
    lv_obj_set_size(btn_save, 140, 55);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -5);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_t* l_save = lv_label_create(btn_save);
    lv_label_set_text(l_save, "Save");
    lv_obj_center(l_save);
    lv_obj_add_event_cb(btn_save, popup_save_cb, LV_EVENT_CLICKED, popup);

    lv_obj_t* btn_cancel = lv_btn_create(popup);
    lv_obj_set_size(btn_cancel, 140, 55);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -5);
    lv_obj_set_style_bg_color(btn_cancel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t* l_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(l_cancel, "Cancel");
    lv_obj_center(l_cancel);
    lv_obj_add_event_cb(btn_cancel, popup_cancel_cb, LV_EVENT_CLICKED, popup);
}

/* ---- Wi-Fi kattintás ---- */
static void wifi_click_cb(lv_event_t* e) {
    char* target_config_str = (char*)lv_event_get_user_data(e);

    lv_obj_t* popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup, 780, 440);
    lv_obj_center(popup);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(popup, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(popup, 2, 0);

    lv_obj_t* title = lv_label_create(popup);
    lv_obj_t* ta = lv_textarea_create(popup);
    lv_obj_set_size(ta, 600, 60);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 60);
    lv_textarea_set_one_line(ta, true);

    if (target_config_str == g_cfg.wifi.ssid) {
        lv_label_set_text(title, "Enter Wi-Fi SSID");
    }
    else {
        lv_label_set_text(title, "Enter Wi-Fi Password");
        lv_textarea_set_password_mode(ta, true);
    }

    lv_textarea_set_text(ta, target_config_str);

    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_set_style_bg_color(ta, lv_color_hex(0x252525), 0);
    lv_obj_set_style_text_color(ta, lv_color_white(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);

    lv_obj_t* kb = lv_keyboard_create(popup);
    lv_obj_set_size(kb, 740, 210);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -65);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_24, LV_PART_ITEMS);

    // Save Gomb
    lv_obj_t* btn_save = lv_btn_create(popup);
    lv_obj_set_size(btn_save, 140, 55);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -5);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_t* l_save = lv_label_create(btn_save);
    lv_label_set_text(l_save, "Save");
    lv_obj_center(l_save);
    lv_obj_add_event_cb(btn_save, popup_wifi_save_cb, LV_EVENT_CLICKED, target_config_str);

    // Cancel Gomb
    lv_obj_t* btn_cancel = lv_btn_create(popup);
    lv_obj_set_size(btn_cancel, 140, 55);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -5);
    lv_obj_set_style_bg_color(btn_cancel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t* l_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(l_cancel, "Cancel");
    lv_obj_center(l_cancel);
    lv_obj_add_event_cb(btn_cancel, popup_cancel_cb, LV_EVENT_CLICKED, popup);
}

/* ---- IP cím kattintás ---- */
static void ip_click_cb(lv_event_t* e) {
    char* target_config_str = (char*)lv_event_get_user_data(e);

    lv_obj_t* popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup, 780, 440);
    lv_obj_center(popup);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(popup, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(popup, 2, 0);

    lv_obj_t* title = lv_label_create(popup);
    lv_label_set_text(title, "Enter IP Address");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* ta = lv_textarea_create(popup);
    lv_obj_set_size(ta, 600, 60);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 60);
    lv_textarea_set_one_line(ta, true);

    lv_textarea_set_text(ta, target_config_str);

    lv_obj_set_style_bg_color(ta, lv_color_hex(0x252525), 0);
    lv_obj_set_style_text_color(ta, lv_color_white(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);

    lv_obj_t* kb = lv_keyboard_create(popup);
    lv_obj_set_size(kb, 740, 210);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -65);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_24, LV_PART_ITEMS);

    // Save Gomb
    lv_obj_t* btn_save = lv_btn_create(popup);
    lv_obj_set_size(btn_save, 140, 55);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -5);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_t* l_save = lv_label_create(btn_save);
    lv_label_set_text(l_save, "Save");
    lv_obj_center(l_save);
    lv_obj_add_event_cb(btn_save, popup_ip_save_cb, LV_EVENT_CLICKED, target_config_str);

    // Cancel Gomb
    lv_obj_t* btn_cancel = lv_btn_create(popup);
    lv_obj_set_size(btn_cancel, 140, 55);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -5);
    lv_obj_set_style_bg_color(btn_cancel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t* l_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(l_cancel, "Cancel");
    lv_obj_center(l_cancel);
    lv_obj_add_event_cb(btn_cancel, popup_cancel_cb, LV_EVENT_CLICKED, popup);
}

/* ---- Segédfüggvények ---- */
static void add_section_title(lv_obj_t* parent, const char* txt) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_pad_top(label, 20, 0);
}

static lv_obj_t* add_setting_row(lv_obj_t* parent, const char* label_txt, const char* value_txt) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 65);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0x404040), 0);

    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_txt);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);

    lv_obj_t* val = lv_label_create(cont);
    lv_label_set_text(val, value_txt);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(val, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

    return val;
}

/* ---- Fõ Képernyõ Létrehozása ---- */
lv_obj_t* screen_page6_create(void) {
    if (s_scr_page6 != NULL) {
        lv_obj_del(s_scr_page6);
        s_scr_page6 = NULL;
    }

    lv_obj_t* scr = lv_obj_create(NULL);
    s_scr_page6 = scr;
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), 0);

    lv_obj_add_event_cb(scr, screen_page6_delete_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t* title = lv_label_create(s_scr_page6);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    setting_list = lv_obj_create(s_scr_page6);
    lv_obj_set_size(setting_list, 770, 380);
    lv_obj_align(setting_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_flex_flow(setting_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(setting_list, 0, 0);
    lv_obj_set_style_border_width(setting_list, 0, 0);
    lv_obj_set_scrollbar_mode(setting_list, LV_SCROLLBAR_MODE_AUTO);

    /* --- WIFI SZEKCIÓ --- */
    add_section_title(setting_list, "WIFI & NETWORK");

    // Wi-Fi SSID -  UGYANAZ A LOGIKA, MINT A JELSZÓNÁL:
    // Ha a mentett SSID üres (0 a hossza), akkor a "[Click to set SSID]" szöveget írjuk ki, különben a valódi nevet.
    char ssid_display[65];
    if (strlen(g_cfg.wifi.ssid) > 0) {
        strcpy(ssid_display, g_cfg.wifi.ssid);
    }
    else {
        strcpy(ssid_display, "[Click to set SSID]");
    }
    lbl_wifi_ssid_val = add_setting_row(setting_list, "Wi-Fi SSID", ssid_display);
    lv_obj_set_style_text_color(lbl_wifi_ssid_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_wifi_ssid_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_wifi_ssid_val, wifi_click_cb, LV_EVENT_CLICKED, g_cfg.wifi.ssid);

    // Wi-Fi Password (dinamikus csillagos verzió)
    size_t pass_len = strlen(g_cfg.wifi.password);
    char stars[65] = "";
    if (pass_len > 0) {
        if (pass_len > 64) pass_len = 64;
        memset(stars, '*', pass_len);
        stars[pass_len] = '\0';
    }
    else {
        strcpy(stars, "[No password]");
    }
    lbl_wifi_password_val = add_setting_row(setting_list, "Wi-Fi Password", stars);
    lv_obj_set_style_text_color(lbl_wifi_password_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_wifi_password_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_wifi_password_val, wifi_click_cb, LV_EVENT_CLICKED, g_cfg.wifi.password);

    /* --- KIJELZÕ SZEKCIÓ --- */
    add_section_title(setting_list, "DISPLAY");
    lv_obj_t* row_bright = lv_obj_create(setting_list);
    lv_obj_set_size(row_bright, lv_pct(100), 65);
    lv_obj_clear_flag(row_bright, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_bright, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_bright, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_bright, lv_color_hex(0x282828), 0);

    lv_obj_t* lbl_b = lv_label_create(row_bright);
    lv_label_set_text(lbl_b, "Brightness");
    lv_obj_set_style_text_font(lbl_b, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_b, lv_color_white(), 0);

    lv_obj_t* slider_b = lv_slider_create(row_bright);
    lv_obj_set_size(slider_b, 200, 15);
    lv_slider_set_range(slider_b, 10, 100);

    /* --- LOKALIZÁCIÓ SZEKCIÓ --- */
    add_section_title(setting_list, "LOCALIZATION");
    add_setting_row(setting_list, "Time Zone", "CET1-CES");

    lv_obj_t* row_city = lv_obj_create(setting_list);
    lv_obj_set_size(row_city, lv_pct(100), 65);
    lv_obj_clear_flag(row_city, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_city, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_city, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_city, lv_color_hex(0x282828), 0);

    lv_obj_t* lbl_left = lv_label_create(row_city);
    lv_label_set_text(lbl_left, "Current City");
    lv_obj_set_style_text_font(lbl_left, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_left, lv_color_white(), 0);

    lbl_current_city_val = lv_label_create(row_city);
    lv_label_set_text(lbl_current_city_val, g_cfg.weather.city);
    lv_obj_set_style_text_font(lbl_current_city_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_current_city_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_current_city_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_current_city_val, city_click_cb, LV_EVENT_CLICKED, NULL);

    /* --- IP CÍMEK SZEKCIÓ --- */
    add_section_title(setting_list, "DEVICE IP ADDRESSES");

    // 1. Fronius IP bekötése
    lbl_inverter_ip_val = add_setting_row(setting_list, "Fronius IP", g_cfg.inverter.ip);
    lv_obj_set_style_text_color(lbl_inverter_ip_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_inverter_ip_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_inverter_ip_val, ip_click_cb, LV_EVENT_CLICKED, g_cfg.inverter.ip);

    // 2. A/C 1 IP bekötése
    lbl_ac1_ip_val = add_setting_row(setting_list, "A/C 1 IP", g_cfg.gree.dev[0].ip);
    lv_obj_set_style_text_color(lbl_ac1_ip_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_ac1_ip_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_ac1_ip_val, ip_click_cb, LV_EVENT_CLICKED, g_cfg.gree.dev[0].ip);

    // 3. A/C 2 IP bekötése
    lbl_ac2_ip_val = add_setting_row(setting_list, "A/C 2 IP", g_cfg.gree.dev[1].ip);
    lv_obj_set_style_text_color(lbl_ac2_ip_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_ac2_ip_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_ac2_ip_val, ip_click_cb, LV_EVENT_CLICKED, g_cfg.gree.dev[1].ip);

    // 4. A/C 3 IP bekötése
    lbl_ac3_ip_val = add_setting_row(setting_list, "A/C 3 IP", g_cfg.gree.dev[2].ip);
    lv_obj_set_style_text_color(lbl_ac3_ip_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_flag(lbl_ac3_ip_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_ac3_ip_val, ip_click_cb, LV_EVENT_CLICKED, g_cfg.gree.dev[2].ip);

    return s_scr_page6;
}