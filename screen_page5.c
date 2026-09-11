#include "screen_page5.h"
#include "ui_common.h"
#include "app.h"
#include "config.h"
#include "gree_backend.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Képek beágyazása */
#include "pictures/img_off.h"
#include "pictures/img_hot.h"
#include "pictures/img_frost.h"

#define AC_UI_MAX 4

static lv_obj_t* s_scr_page5;
static lv_timer_t* s_status_timer = NULL;

/* Stílusok */
static lv_style_t style_card_idle;
static lv_style_t style_card_cool;
static lv_style_t style_card_heat;
static bool styles_inited = false;

typedef struct {
    lv_obj_t* card;
    lv_obj_t* ip_lbl;
    lv_obj_t* power_sw;
    lv_obj_t* temp_slider;
    lv_obj_t* temp_val_lbl;
    lv_obj_t* status_lbl;
    lv_obj_t* icon_obj;    /* ÚJ: Az ikon tárolásához */
} ac_ui_t;

static ac_ui_t s_ac[AC_UI_MAX];
static int s_ac_count = 0;

/* ---- Stílusok inicializálása ---- */
static void init_custom_styles(void) {
    if (styles_inited) return;

    lv_style_init(&style_card_idle);
    lv_style_set_radius(&style_card_idle, 16);
    lv_style_set_bg_opa(&style_card_idle, LV_OPA_COVER);
    lv_style_set_bg_color(&style_card_idle, lv_color_hex(0x414441));
    lv_style_set_border_width(&style_card_idle, 0);
    lv_style_set_text_color(&style_card_idle, lv_color_white());

    lv_style_init(&style_card_cool);
    lv_style_copy(&style_card_cool, &style_card_idle);
    lv_style_set_bg_color(&style_card_cool, lv_color_hex(0x2E5B88));

    lv_style_init(&style_card_heat);
    lv_style_copy(&style_card_heat, &style_card_idle);
    lv_style_set_bg_color(&style_card_heat, lv_color_hex(0x882E2E));

    styles_inited = true;
}

/* ---- Callbackek (maradtak a régiek) ---- */
static void power_sw_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_ac_count) return;
    const gree_dev_t* dev = &g_cfg.gree.dev[idx];
    lv_obj_t* sw = lv_event_get_target(e);
    gree_power_set(dev, lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void temp_slider_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_ac_count) return;
    const gree_dev_t* dev = &g_cfg.gree.dev[idx];
    int new_temp = lv_slider_get_value(lv_event_get_target(e));
    gree_temp_set(dev, new_temp);
    lv_label_set_text_fmt(s_ac[idx].temp_val_lbl, "%d°C", new_temp);
}

/* ---- Időzítő: státusz, színek ÉS ikonok frissítése ---- */
static void update_gree_status_timer(lv_timer_t* t) {
    for (int i = 0; i < s_ac_count; i++) {
        const gree_dev_t* dev = &g_cfg.gree.dev[i];
        gree_status_t st;

        if (!gree_status_get(dev, &st)) {
            lv_obj_remove_style(s_ac[i].card, &style_card_cool, LV_PART_MAIN);
            lv_obj_remove_style(s_ac[i].card, &style_card_heat, LV_PART_MAIN);
            lv_obj_add_style(s_ac[i].card, &style_card_idle, LV_PART_MAIN);
            lv_image_set_src(s_ac[i].icon_obj, &img_off); /* Offline -> OFF ikon */

            lv_label_set_text(s_ac[i].status_lbl, "Offline");
            lv_obj_add_state(s_ac[i].power_sw, LV_STATE_DISABLED);
            lv_obj_add_state(s_ac[i].temp_slider, LV_STATE_DISABLED);
            continue;
        }

        lv_obj_clear_state(s_ac[i].power_sw, LV_STATE_DISABLED);
        lv_obj_clear_state(s_ac[i].temp_slider, LV_STATE_DISABLED);

        if (st.power) lv_obj_add_state(s_ac[i].power_sw, LV_STATE_CHECKED);
        else lv_obj_clear_state(s_ac[i].power_sw, LV_STATE_CHECKED);

        lv_slider_set_value(s_ac[i].temp_slider, st.set_temp, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_ac[i].temp_val_lbl, "%d°C", st.set_temp);
        lv_label_set_text_fmt(s_ac[i].status_lbl, "Room: %d°C", st.room_temp);

        /* SZÍNVÁLTÁS ÉS IKON LOGIKA */
        lv_obj_remove_style(s_ac[i].card, &style_card_cool, LV_PART_MAIN);
        lv_obj_remove_style(s_ac[i].card, &style_card_heat, LV_PART_MAIN);
        lv_obj_remove_style(s_ac[i].card, &style_card_idle, LV_PART_MAIN);

        if (!st.power) {
            lv_obj_add_style(s_ac[i].card, &style_card_idle, LV_PART_MAIN);
            lv_image_set_src(s_ac[i].icon_obj, &img_off);
        }
        else {
            if (st.room_temp > st.set_temp) {
                lv_obj_add_style(s_ac[i].card, &style_card_cool, LV_PART_MAIN);
                lv_image_set_src(s_ac[i].icon_obj, &img_frost);
            }
            else if (st.room_temp < st.set_temp) {
                lv_obj_add_style(s_ac[i].card, &style_card_heat, LV_PART_MAIN);
                lv_image_set_src(s_ac[i].icon_obj, &img_hot);
            }
            else {
                lv_obj_add_style(s_ac[i].card, &style_card_idle, LV_PART_MAIN);
                lv_image_set_src(s_ac[i].icon_obj, &img_off);
            }
        }
    }
}

static void screen_page5_delete_cb(lv_event_t* e) {
    if (s_status_timer) {
        lv_timer_del(s_status_timer);
        s_status_timer = NULL;
    }
}

lv_obj_t* screen_page5_create(void) {
    init_custom_styles();
    memset(s_ac, 0, sizeof(s_ac));
    s_ac_count = 0;

    lv_obj_t* scr = lv_obj_create(NULL);
    s_scr_page5 = scr;
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, screen_page5_delete_cb, LV_EVENT_DELETE, NULL);

    for (int i = 0; i < GREE_MAX && s_ac_count < 3; i++) {
        if (g_cfg.gree.dev[i].ip[0] != '\0') s_ac_count++;
    }

    if (s_ac_count == 0) {
        lv_obj_t* no_dev_lbl = lv_label_create(scr);
        lv_label_set_text(no_dev_lbl, "Nincs konfiguralt Gree eszkoz!");
        lv_obj_set_style_text_font(no_dev_lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(no_dev_lbl, lv_color_white(), 0);
        lv_obj_align(no_dev_lbl, LV_ALIGN_CENTER, 0, 0);
        return scr;
    }

    const int card_width = 250;
    const int card_height = 450;
    const int start_y = 15;
    int x_positions[3] = { 14, 400 - (card_width / 2), 800 - card_width - 14 };

    for (int i = 0; i < 3; i++) {
        lv_obj_t* card = lv_obj_create(scr);
        s_ac[i].card = card;
        lv_obj_set_size(card, card_width, card_height);
        lv_obj_set_pos(card, x_positions[i], start_y);
        lv_obj_add_style(card, &style_card_idle, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        const char* dev_ip = (i < s_ac_count) ? g_cfg.gree.dev[i].ip : "0.0.0.0";

        // IP Cím
        lv_obj_t* ip_lbl = lv_label_create(card);
        lv_label_set_text(ip_lbl, dev_ip);
        lv_obj_set_style_text_font(ip_lbl, &lv_font_montserrat_28, 0);
        lv_obj_align(ip_lbl, LV_ALIGN_TOP_MID, 0, -5);
        s_ac[i].ip_lbl = ip_lbl;

        /* ---- IKON LÉTREHOZÁSA (Page 4 fix koordináta logika) ---- */
        s_ac[i].icon_obj = lv_image_create(s_ac[i].card);
        lv_image_set_src(s_ac[i].icon_obj, &img_off);

        // Fix méret 128x128-as forrás esetén, 80x80-ra skálázva
        lv_obj_set_size(s_ac[i].icon_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // FIX KOORDINÁTA: 
        // X: Kártya közepe (250/2 = 125, mínusz az ikon fele (40) = 85)
        // Y: A hőmérséklet érték (180) és a Room temp (kb 400) közé tesszük: 280
        lv_obj_set_pos(s_ac[i].icon_obj, 45, 230);

        // Page 4-ben használt "brute force" újraszínezés
        lv_obj_set_style_image_recolor(s_ac[i].icon_obj, lv_color_white(), 0);
        lv_obj_set_style_image_recolor_opa(s_ac[i].icon_obj, LV_OPA_COVER, 0);
        // Ez volt a Page 4 titka: kényszerített alapértelmezett állapot színezés
        lv_obj_set_style_image_recolor(s_ac[i].icon_obj, lv_color_white(), LV_STATE_DEFAULT);

        // Power felirat
        lv_obj_t* power_lbl = lv_label_create(card);
        lv_label_set_text(power_lbl, "Power");
        lv_obj_set_style_text_font(power_lbl, &lv_font_montserrat_24, 0);
        lv_obj_align(power_lbl, LV_ALIGN_TOP_LEFT, 10, 50);

        // Switch
        s_ac[i].power_sw = lv_switch_create(card);
        lv_obj_set_size(s_ac[i].power_sw, 80, 42);
        lv_obj_align(s_ac[i].power_sw, LV_ALIGN_TOP_RIGHT, -10, 45);
        lv_obj_add_event_cb(s_ac[i].power_sw, power_sw_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_color(s_ac[i].power_sw, lv_color_hex(0x4CAF50), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(s_ac[i].power_sw, lv_color_white(), LV_PART_KNOB);

        // Hőmérséklet felirat
        lv_obj_t* temp_lbl = lv_label_create(card);
        lv_label_set_text(temp_lbl, "Temperature:");
        lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_24, 0);
        lv_obj_align(temp_lbl, LV_ALIGN_TOP_LEFT, 10, 100);

        // Slider
        s_ac[i].temp_slider = lv_slider_create(card);
        lv_obj_set_size(s_ac[i].temp_slider, 200, 24);
        lv_obj_align(s_ac[i].temp_slider, LV_ALIGN_TOP_MID, 0, 140);
        lv_slider_set_range(s_ac[i].temp_slider, 18, 30);
        lv_slider_set_value(s_ac[i].temp_slider, 22, LV_ANIM_OFF);
        lv_obj_add_event_cb(s_ac[i].temp_slider, temp_slider_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_color(s_ac[i].temp_slider, lv_color_hex(0xDDDDDD), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_ac[i].temp_slider, lv_color_white(), LV_PART_KNOB);

        // Hőmérséklet érték
        s_ac[i].temp_val_lbl = lv_label_create(card);
        lv_label_set_text(s_ac[i].temp_val_lbl, "22°C");
        lv_obj_set_style_text_font(s_ac[i].temp_val_lbl, &lv_font_montserrat_28, 0);
        lv_obj_align(s_ac[i].temp_val_lbl, LV_ALIGN_TOP_MID, 0, 180);

        // Státusz (Room temp)
        s_ac[i].status_lbl = lv_label_create(card);
        lv_label_set_text(s_ac[i].status_lbl, "Room: --");
        lv_obj_set_style_text_font(s_ac[i].status_lbl, &lv_font_montserrat_24, 0);
        lv_obj_align(s_ac[i].status_lbl, LV_ALIGN_BOTTOM_MID, 0, 0);

        // Kényszerített frissítés, hogy az I1 kép biztosan megjelenjen az első renderelésnél
        lv_obj_invalidate(s_ac[i].icon_obj);
    }

    s_status_timer = lv_timer_create(update_gree_status_timer, 3000, NULL);
    return scr;
}