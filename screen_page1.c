#include "screen_page1.h"
#include "app.h"
#include "config.h"
#include "ui_common.h"
#include "time_service.h"
#include "fonts/quartz_60.h"
#include "fonts/quartz_90.h"
#include "fonts/quartz_140.h"
#include "fonts/quartz_200.h"

extern app_config_t g_cfg; // Ez a globális config, amiben benne van az időzóna offset is (g_cfg.weather.utc_offset_sec)

/* ---- Globális UI elemek ---- */
static lv_obj_t* lbl_tz;
static lv_obj_t* lbl_time;
static lv_obj_t* lbl_date;
static lv_obj_t* lbl_sec;

/* ---- Időzítő pointer ---- */
static lv_timer_t* page1_timer = NULL;

/* ---- Képernyő pointer (frissítéshez) ---- */
static lv_obj_t* s_scr_page1 = NULL;

/* ---- Idő frissítő callback ---- */
static void page1_time_update_cb(lv_timer_t* t)
{
    if (lv_scr_act() != s_scr_page1) return;

    /* 1. Lekérjük a nyers UTC időt a Windowstól */
    SYSTEMTIME st;
    GetSystemTime(&st);

    /* 2. Átváltjuk olyan formátumba, amivel tudunk számolni (Unix timestamp) */
    struct tm utc_tm = { 0 };
    utc_tm.tm_year = st.wYear - 1900;
    utc_tm.tm_mon = st.wMonth - 1;
    utc_tm.tm_mday = st.wDay;
    utc_tm.tm_hour = st.wHour;
    utc_tm.tm_min = st.wMinute;
    utc_tm.tm_sec = st.wSecond;
    utc_tm.tm_isdst = -1;

    time_t utc_ts = _mkgmtime(&utc_tm); // UTC timestamp

    /* 3. Hozzáadjuk a configban tárolt eltolást (amit a Page3 mentett el) */
    // g_cfg.weather.utc_offset_sec tartalmazza pl. a 7200-at Budapestnél
    time_t local_ts = utc_ts + g_cfg.weather.utc_offset_sec;

    /* 4. Visszaalakítjuk olvasható struktúrává */
    struct tm* now = gmtime(&local_ts);

    /* ---- Innentől jön a kiíratás (a te eredeti kódod) ---- */
    static const char* days[] = { "SUN", "MON", "TUE", "WED", "TRU", "FRI", "SAT" };

    lv_label_set_text_fmt(lbl_date, "%04d.%02d.%02d %s",
        now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, days[now->tm_wday]);

    lv_label_set_text_fmt(lbl_time, "%02d:%02d", now->tm_hour, now->tm_min);
    lv_label_set_text_fmt(lbl_sec, "%02d", now->tm_sec);
}

/* ---- Képernyő törlésekor időzítő leállítása ---- */
static void screen_page1_delete_cb(lv_event_t* e)
{
    if (page1_timer) {
        lv_timer_del(page1_timer);
        page1_timer = NULL;
    }
}

static void format_timezone_str(char* str) {
    if (str == NULL) return;

    // 1. Ékezetmentesítés (a config.c-ben lévő közös függvényt használjuk)
    strip_accents(str);

    // 2. Nagybetűssé alakítás
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = str[i] - 32;
        }
    }
}

/* ---- Page1 (Home) képernyő ---- */
lv_obj_t* screen_page1_create(void)
{
    lv_obj_t* scr = lv_obj_create(NULL); 
    s_scr_page1 = scr;
    lv_obj_set_style_pad_all(scr, 20, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);    
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(scr, screen_page1_delete_cb, LV_EVENT_DELETE, NULL);
    
    /* ---- IDŐ + DÁTUM KÁRTYA ---- */
    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 770, 450);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x849058), 0); // reverse (0x313137)
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);    

    /* ---- DÁTUM + HÉT NAPJA ---- */
    lbl_date = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_date, &quartz_90, 0);
    lv_label_set_text(lbl_date, "0000.00.00 000");
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 25);
    //lv_obj_set_style_text_color(lbl_date, lv_color_hex(0x849058), 0);    

    /* ---- IDŐ (óra:perc) ---- */
    lbl_time = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_time, &quartz_200, 0);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_align(lbl_time, LV_ALIGN_TOP_LEFT, 0, 145);
    //lv_obj_set_style_text_color(lbl_time, lv_color_hex(0x849058), 0);

    /* ---- MÁSODPERC ---- */
    lbl_sec = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_sec, &quartz_140, 0);
    lv_label_set_text(lbl_sec, "00");
    lv_obj_align(lbl_sec, LV_ALIGN_TOP_RIGHT, 0, 185);
    //lv_obj_set_style_text_color(lbl_sec, lv_color_hex(0x849058), 0);

    /* ---- IDŐZÓNA LABEL a kártyán belül ---- */
    lbl_tz = lv_label_create(card); // <--- Itt a 'card' a parent!
    lv_obj_set_style_text_font(lbl_tz, &quartz_60, 0);    

    if (strlen(g_cfg.weather.timezone) > 0) {
        char tz_upper[64];
        strncpy(tz_upper, g_cfg.weather.timezone, sizeof(tz_upper) - 1);
        tz_upper[sizeof(tz_upper) - 1] = '\0';

        // Formázás: Nagybetű + Ékezetmentes
        format_timezone_str(tz_upper);

        lv_label_set_text(lbl_tz, tz_upper);
    }
    else {
        lv_label_set_text(lbl_tz, "LOCAL TIME");
    }
    lv_obj_align(lbl_tz, LV_ALIGN_TOP_MID, 0, 355);

    /* ---- Idő frissítő timer ---- */
    page1_timer = lv_timer_create(page1_time_update_cb, 1000, NULL);

    s_scr_page1 = scr;
    return scr;
}