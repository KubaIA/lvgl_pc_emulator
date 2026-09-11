#include "screen_page3.h"
#include "ui_common.h"
#include "app.h"
#include "config.h"

#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cJSON.h"

#include "fonts/lv_montserrat_90.h"

/* ---- Weather icons ---- */
#include "pictures/img_00_clear_day.h"
#include "pictures/img_00_clear_night.h"
#include "pictures/img_01_mainly_clear_day.h"
#include "pictures/img_01_mainly_clear_night.h"
#include "pictures/img_02_partly_clear_day.h"
#include "pictures/img_02_partly_clear_night.h"
#include "pictures/img_03_overcast.h"
#include "pictures/img_45_fog.h"
#include "pictures/img_61_rain_light_day.h"
#include "pictures/img_61_rain_light_night.h"
#include "pictures/img_63_rain_medium.h"
#include "pictures/img_65_rain_heavy.h"
#include "pictures/img_66_sleet.h"
#include "pictures/img_71_snow.h"
#include "pictures/img_95_thunderstorm.h"
#include "pictures/img_96_thunderstorm_hail.h"
#include "pictures/img_unknown.h"

#pragma comment(lib, "winhttp.lib")

/* ---- Weather code -> english text ---- */
static const char* weathercode_to_text(int code)
{
    switch (code)
    {
    case 0:  return "Clear";
    case 1:  return "Mostly Clear";
    case 2:  return "Partly Cloudy";
    case 3:  return "Overcast";

    case 45: return "Fog";
    case 48: return "Freezing Fog";

    case 51: return "Light Drizzle";
    case 53: return "Moderate Drizzle";
    case 55: return "Heavy Drizzle";

    case 56: return "Light Freezing Drizzle";
    case 57: return "Freezing Drizzle";

    case 61: return "Light Rain";
    case 63: return "Moderate Rain";
    case 65: return "Heavy Rain";

    case 66: return "Light Freezing Rain";
    case 67: return "Freezing Rain";

    case 71: return "Light Snow";
    case 73: return "Snow";
    case 75: return "Heavy Snow";
    case 77: return "Snow Grains";

    case 80: return "Light Rain Shower";
    case 81: return "Rain Shower";
    case 82: return "Heavy Rain Shower";

    case 85: return "Snow Shower";
    case 86: return "Heavy Snow Shower";

    case 95: return "Thunderstorm";
    case 96: return "Thunderstorm with Hail";
    case 99: return "Severe Thunderstorm with Hail";

    default: return "Unknown";
    }
}

/* ---- Weather code -> icon ---- */
static const lv_image_dsc_t* weathercode_to_icon(int code, bool is_day)
{
    switch (code)
    {
    case 0:  return is_day ? &img_00_clear_day : &img_00_clear_night;
    case 1:  return is_day ? &img_01_mainly_clear_day : &img_01_mainly_clear_night;
    case 2:  return is_day ? &img_02_partly_clear_day : &img_02_partly_clear_night;
    case 3:  return &img_03_overcast;

    case 45: return &img_45_fog;
    case 48: return &img_45_fog;

    case 51: return is_day ? &img_61_rain_light_day : &img_61_rain_light_night;
    case 53: return &img_63_rain_medium;
    case 55: return &img_65_rain_heavy;

    case 56: return &img_66_sleet;
    case 57: return &img_66_sleet;

    case 61: return is_day ? &img_61_rain_light_day : &img_61_rain_light_night;
    case 63: return &img_63_rain_medium;
    case 65: return &img_65_rain_heavy;

    case 66: return &img_66_sleet;
    case 67: return &img_66_sleet;

    case 71: return &img_71_snow;
    case 73: return &img_71_snow;
    case 75: return &img_71_snow;
    case 77: return &img_71_snow;

    case 80: return is_day ? &img_61_rain_light_day : &img_61_rain_light_night;
    case 81: return &img_63_rain_medium;
    case 82: return &img_65_rain_heavy;

    case 85: return &img_71_snow;
    case 86: return &img_71_snow;

    case 95: return &img_95_thunderstorm;
    case 96: return &img_96_thunderstorm_hail;
    case 99: return &img_96_thunderstorm_hail;

    default: return &img_unknown;
    }
}

/* ---- Open-Meteo ---- */
#define WEATHER_HOST   L"api.open-meteo.com"
#define GEO_HOST       L"geocoding-api.open-meteo.com"

static wchar_t s_weather_path[256];
static lv_obj_t* s_scr_page3;

/* ---- Stílus Kártya ---- */
static lv_style_t style_card;
static bool style_card_inited = false;

/* UI elemek */
static lv_obj_t* card_weather;
static lv_obj_t* lbl_city;
static lv_obj_t* line_city_separator;
static lv_obj_t* img_weather;
static lv_obj_t* lbl_temp;
static lv_obj_t* lbl_hum;
static lv_obj_t* lbl_wind;
static lv_obj_t* lbl_code;
static lv_obj_t* lbl_uv;

/* Aktuális koordináták és városnév */
static double current_lat = 47.4979;
static double current_lon = 19.0402;
static char current_city[64] = "Budapest";

/* ---- Dinamikus stílusok deklarálása ---- */
static lv_style_t style_city_label_small; // Kisebb font a hosszú nevekhez
static lv_style_t style_city_label;       // Eredeti font
static bool styles_inited = false;        // Stílus állapot jelző

/* Előre deklaráció */
static void update_weather_cb(lv_timer_t* t);

/* Timer pointer a frissítéshez */
static lv_timer_t* weather_timer = NULL;

/* ==== Általános HTTPS GET adott host + path-hoz ==== */
char* https_get_host_path(const wchar_t* host, const wchar_t* path)
{
    HINTERNET hSession = WinHttpOpen(L"LVGL Weather",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;

    // Timeout-ok beállítása: Resolve (2s), Connect (2s), Send (2s), Receive (2s)
    // Így ha nincs net vagy lassú a szerver, nem fagy ki az UI hosszú időre
    WinHttpSetTimeouts(hSession, 2000, 2000, 2000, 2000);
    /* ---------------------------------- */

    HINTERNET hConnect = WinHttpConnect(hSession,
        host,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"GET",
        path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    BOOL ok = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0,
        0, 0);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD size = 0;
    DWORD downloaded = 0;
    char* chunk = NULL;
    char* result = NULL;
    size_t total = 0;

    for (;;) {
        if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
        if (size == 0) break;

        chunk = (char*)malloc(size + 1);
        if (!chunk) break;

        if (!WinHttpReadData(hRequest, chunk, size, &downloaded)) {
            free(chunk);
            break;
        }

        chunk[downloaded] = '\0';

        char* newbuf = (char*)realloc(result, total + downloaded + 1);
        if (!newbuf) {
            free(chunk);
            free(result);
            result = NULL;
            break;
        }

        result = newbuf;
        memcpy(result + total, chunk, downloaded);
        total += downloaded;
        result[total] = '\0';

        free(chunk);
        chunk = NULL;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

/* ---- HTTPS GET → Open-Meteo forecast ---- */
static char* https_get_open_meteo(void)
{
    return https_get_host_path(WEATHER_HOST, s_weather_path);
}

/* ==== Városnév → koordináta (forward geocoding) ==== */
bool geocode_city(const char* city_name, double* out_lat, double* out_lon, char* out_display_name, size_t name_sz)
{
    if (!city_name || !out_lat || !out_lon) return false;

    wchar_t encoded_city[256];
    swprintf(encoded_city, 256, L"%S", city_name);

    for (int i = 0; encoded_city[i]; i++) {
        if (encoded_city[i] == L' ') {
            encoded_city[i] = L'+';
        }
    }

    wchar_t path[256];
    swprintf(path,
        (sizeof(path) / sizeof(path[0])),
        L"/v1/search?name=%s&count=1&language=hu",
        encoded_city);

    char* json_text = https_get_host_path(GEO_HOST, path);
    if (!json_text) return false;

    cJSON* root = cJSON_Parse(json_text);
    free(json_text);
    if (!root) return false;

    cJSON* results = cJSON_GetObjectItem(root, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        cJSON_Delete(root);
        return false;
    }

    cJSON* first = cJSON_GetArrayItem(results, 0);
    if (!cJSON_IsObject(first)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON* name = cJSON_GetObjectItem(first, "name");
    cJSON* lat = cJSON_GetObjectItem(first, "latitude");
    cJSON* lon = cJSON_GetObjectItem(first, "longitude");
    cJSON* tz = cJSON_GetObjectItem(first, "timezone");

    if (!cJSON_IsString(name) || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        cJSON_Delete(root);
        return false;
    }

    if (cJSON_IsString(tz) && tz->valuestring) {
        strncpy(g_cfg.weather.timezone, tz->valuestring, sizeof(g_cfg.weather.timezone) - 1);
        g_cfg.weather.timezone[sizeof(g_cfg.weather.timezone) - 1] = '\0';
    }

    *out_lat = lat->valuedouble;
    *out_lon = lon->valuedouble;

    if (out_display_name && name_sz > 0) {
        strncpy(out_display_name, name->valuestring, name_sz - 1);
        out_display_name[name_sz - 1] = '\0';

        // Itt hívjuk meg a tisztítást!
        strip_accents(out_display_name);
    }

    cJSON_Delete(root);
    return true;
}

/* ---- JSON feldolgozás ---- */
static void update_weather_cb(lv_timer_t* t)
{
    (void)t;

    // 1. A hálózati kérést MINDIG elindítjuk, függetlenül attól, melyik oldalon vagyunk
    char* json_text = https_get_open_meteo();
    if (!json_text) return;

    cJSON* root = cJSON_Parse(json_text);
    free(json_text);
    if (!root) return;

    // 2. AZ IDŐZÓNA OFFSET KINYERÉSE (Ez a legfontosabb a Page 1 órájának!)
    // Ez a gyökérben (root) van, nem a current-ben.
    cJSON* utc_offset = cJSON_GetObjectItem(root, "utc_offset_seconds");
    if (cJSON_IsNumber(utc_offset)) {
        g_cfg.weather.utc_offset_sec = utc_offset->valueint;
        // Mentés, hogy indításkor a fájlból is jó legyen
        config_save_json(&g_cfg, "config.json");
    }

    // 3. CSAK AKKOR FRISSÍTJÜK A PAGE 3 ELEMEIT, HA AZ AKTÍV!
    // Ha nem ezen az oldalon vagyunk, a többi JSON adatot nem is kell feldolgoznunk.
    if (lv_scr_act() == s_scr_page3) {

        cJSON* current = cJSON_GetObjectItem(root, "current");
        if (!cJSON_IsObject(current)) {
            cJSON_Delete(root);
            return;
        }

        cJSON* t2m = cJSON_GetObjectItem(current, "temperature_2m");
        cJSON* rh = cJSON_GetObjectItem(current, "relative_humidity_2m");
        cJSON* wind = cJSON_GetObjectItem(current, "windspeed_10m");
        cJSON* winddir = cJSON_GetObjectItem(current, "winddirection_10m");
        cJSON* wcod = cJSON_GetObjectItem(current, "weathercode");
        cJSON* uv = cJSON_GetObjectItem(current, "uv_index");
        cJSON* is_day = cJSON_GetObjectItem(current, "is_day");

        if (cJSON_IsNumber(t2m) && cJSON_IsNumber(rh) && cJSON_IsNumber(wind)) {
            double temp = t2m->valuedouble;
            double ws = wind->valuedouble;
            int hum = (int)(rh->valuedouble + 0.5);
            int code = (int)(wcod->valuedouble + 0.5);
            int dir = (int)(winddir->valuedouble + 0.5);
            double uv_val = uv->valuedouble;
            int uv10 = (int)(uv_val * 10.0 + 0.5);

            int temp10 = (int)(temp * 10.0);
            int ws10 = (int)(ws * 10.0);
            bool day = cJSON_IsNumber(is_day) && is_day->valueint == 1;

            // Kártya színe nappal/éjjel
            if (day) {
                lv_obj_set_style_bg_color(card_weather, lv_color_hex(0x87CEFA), 0);
                lv_obj_set_style_bg_grad_color(card_weather, lv_color_hex(0x37c4e8), 0);
            }
            else {
                lv_obj_set_style_bg_color(card_weather, lv_color_hex(0x001F72), 0);
                lv_obj_set_style_bg_grad_color(card_weather, lv_color_hex(0x001F3F), 0);
            }
            lv_obj_set_style_bg_grad_dir(card_weather, LV_GRAD_DIR_VER, 0);

            // Szövegszín igazítása
            lv_color_t text_color = day ? lv_color_hex(0x2b3990) : lv_color_white();
            lv_obj_set_style_text_color(lbl_city, text_color, 0);
            lv_obj_set_style_text_color(lbl_temp, text_color, 0);
            lv_obj_set_style_text_color(lbl_hum, text_color, 0);
            lv_obj_set_style_text_color(lbl_wind, text_color, 0);
            lv_obj_set_style_text_color(lbl_code, text_color, 0);
            lv_obj_set_style_text_color(lbl_uv, text_color, 0);
            lv_obj_set_style_bg_color(line_city_separator, text_color, 0);

            // Adatok kiírása
            lv_img_set_src(img_weather, weathercode_to_icon(code, day));
            lv_label_set_text_fmt(lbl_temp, "%d.%d°C", temp10 / 10, abs(temp10 % 10));
            lv_label_set_text_fmt(lbl_hum, "Humidity: %d %%", hum);
            lv_label_set_text_fmt(lbl_wind, "Wind: %d.%dm/s  %d%c°", ws10 / 10, abs(ws10 % 10), dir, 176);
            lv_label_set_text_fmt(lbl_code, "%s", weathercode_to_text(code));
            lv_label_set_text_fmt(lbl_uv, "UV index: %d.%d", uv10 / 10, abs(uv10 % 10));
        }
    }

    cJSON_Delete(root);
}

/* ---- Képernyő törlése ---- */
static void screen_page3_delete_cb(lv_event_t* e)
{
    (void)e;

    if (weather_timer) {
        lv_timer_del(weather_timer);
        weather_timer = NULL;
    }
}

/* ---- Segédfüggvény a városnév fontméretének váltásához ---- */
static void update_city_label_style(const char* name) {
    if (lbl_city == NULL) return;

    size_t len = strlen(name);

    /* Töröljük az aktuális font stílusokat */
    lv_obj_remove_style(lbl_city, &style_city_label, 0);
    lv_obj_remove_style(lbl_city, &style_city_label_small, 0);

    /* 13 karakternél váltunk */
    if (len > 13) {
        lv_obj_add_style(lbl_city, &style_city_label_small, 0);
    }
    else {
        lv_obj_add_style(lbl_city, &style_city_label, 0);
    }
}

/* Ezt a függvényt hívhatod meg a Page6-ból mentés után */
void screen_page3_force_update_location(void) {
    const char* cfg_city = g_cfg.weather.city;
    double lat, lon;
    char display[64];

    if (geocode_city(cfg_city, &lat, &lon, display, sizeof(display))) {
        current_lat = lat;
        current_lon = lon;

        strncpy(current_city, display, sizeof(current_city) - 1);
        current_city[sizeof(current_city) - 1] = '\0';
        strip_accents(current_city);

        // EZ A LÉNYEG: Itt is állítsuk össze az útvonalat!
        swprintf(s_weather_path, 256,
            L"/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,windspeed_10m,winddirection_10m,uv_index,weathercode,is_day",
            current_lat, current_lon);

        if (lbl_city) {
            lv_label_set_text(lbl_city, current_city);
            update_city_label_style(current_city);
        }

        // Most már bátran hívhatjuk, az s_weather_path készen áll
        update_weather_cb(NULL);
    }
}

/* ---- Képernyő létrehozása ---- */
lv_obj_t* screen_page3_create(void)
{
    lv_obj_t* scr = lv_obj_create(NULL);
    s_scr_page3 = scr;

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(scr, screen_page3_delete_cb, LV_EVENT_DELETE, NULL);

    if (!style_card_inited) {
        style_card_inited = true;
        lv_style_init(&style_card);
        lv_style_set_radius(&style_card, 16);
        lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
        lv_style_set_bg_color(&style_card, lv_color_hex(0x414441));
        lv_style_set_border_width(&style_card, 1);
        lv_style_set_border_color(&style_card, lv_color_hex(0xD0D0D0));
        lv_style_set_pad_all(&style_card, 10);
    }

    /* ---- Város meghatározása config alapján ---- */
    const char* cfg_city = g_cfg.weather.city;
    if (cfg_city && cfg_city[0] != '\0') {
        // Először beállítjuk az alapértelmezettet a configból
        strncpy(current_city, cfg_city, sizeof(current_city) - 1);
        current_city[sizeof(current_city) - 1] = '\0';

        double lat, lon;
        char display[64];
        if (geocode_city(cfg_city, &lat, &lon, display, sizeof(display))) {
            current_lat = lat;
            current_lon = lon;

            // Itt is a tisztított nevet használjuk a megjelenítéshez
            strncpy(current_city, display, sizeof(current_city) - 1);
            current_city[sizeof(current_city) - 1] = '\0';
            strip_accents(current_city); // Biztos ami biztos
        }
    }

    swprintf(s_weather_path,
        (sizeof(s_weather_path) / sizeof(s_weather_path[0])),
        L"/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,windspeed_10m,winddirection_10m,uv_index,weathercode,is_day",
        current_lat, current_lon);

    /* ---- Kártya ---- */
    card_weather = lv_obj_create(scr);
    lv_obj_remove_style_all(card_weather);
    lv_obj_add_style(card_weather, &style_card, 0);
    lv_obj_clear_flag(card_weather, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card_weather, 15, 15);
    lv_obj_set_size(card_weather, 800 - 30, 480 - 15 - 15);
    lv_obj_set_style_border_width(card_weather, 0, 0);

    /* ---- Külön stílusok, hogy a fontok később egyszerűen cserélhetők legyenek ---- */
    static lv_style_t style_city_label;
    static lv_style_t style_weather_labels;
    static lv_style_t style_code_labels;
    static lv_style_t style_temp_labels;
    static bool styles_inited = false;

    /* ---- Külön stílusok inicializálása ---- */
    if (!styles_inited) {
        styles_inited = true;

        /* Normál méret (Eredeti) */
        lv_style_init(&style_city_label);
        lv_style_set_text_font(&style_city_label, &lv_font_montserrat_40);

        /* Kisebb méret (Hosszú nevekhez) */
        lv_style_init(&style_city_label_small);
        lv_style_set_text_font(&style_city_label_small, &lv_font_montserrat_28);

        /* Többi marad változatlan */
        lv_style_init(&style_weather_labels);
        lv_style_set_text_font(&style_weather_labels, &lv_font_montserrat_32);

        lv_style_init(&style_code_labels);
        lv_style_set_text_font(&style_code_labels, &lv_font_montserrat_36);

        lv_style_init(&style_temp_labels);
        lv_style_set_text_font(&style_temp_labels, &lv_montserrat_90);
    }

    /* ---- Egyszerű függőleges vonal ---- */
    line_city_separator = lv_obj_create(card_weather);
    lv_obj_remove_style_all(line_city_separator);
    lv_obj_clear_flag(line_city_separator, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* MÉRET: első szám = vastagság, második szám = magasság */
    lv_obj_set_size(line_city_separator, 7, 330);

    /* ELHELYEZÉS: */
    lv_obj_align(line_city_separator, LV_ALIGN_TOP_MID, 0, 50);

    /* SZÍN: ezt itt tudod később könnyen átírni */
    lv_obj_set_style_bg_color(line_city_separator, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(line_city_separator, LV_OPA_60, 0);

    /* ha teljesen éles vonalat akarsz */
    lv_obj_set_style_radius(line_city_separator, 0, 0);

    /* ---- Ikon ---- */
    img_weather = lv_img_create(card_weather);
    lv_img_set_src(img_weather, &img_unknown);
    lv_obj_align(img_weather, LV_ALIGN_TOP_LEFT, 110, 190);
    lv_img_set_zoom(img_weather, 450);

    /* ---- Label-ek ---- */
    lbl_city = lv_label_create(card_weather);
    lbl_temp = lv_label_create(card_weather);
    lbl_hum = lv_label_create(card_weather);
    lbl_wind = lv_label_create(card_weather);
    lbl_uv = lv_label_create(card_weather);
    lbl_code = lv_label_create(card_weather);

    lv_obj_add_style(lbl_city, &style_city_label, 0);
    lv_obj_add_style(lbl_temp, &style_temp_labels, 0);
    lv_obj_add_style(lbl_hum, &style_weather_labels, 0);
    lv_obj_add_style(lbl_wind, &style_weather_labels, 0);    
    lv_obj_add_style(lbl_uv, &style_weather_labels, 0);
    lv_obj_add_style(lbl_code, &style_code_labels, 0);

    /* ELHELYEZÉS: */
    lv_obj_align(lbl_city, LV_ALIGN_TOP_LEFT, 430, 40);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_LEFT, 430, 130);
    lv_obj_align(lbl_hum, LV_ALIGN_TOP_LEFT, 430, 245);
    lv_obj_align(lbl_wind, LV_ALIGN_TOP_LEFT, 430, 295);    
    lv_obj_align(lbl_uv, LV_ALIGN_TOP_LEFT, 430, 345);
    lv_obj_align(lbl_code, LV_ALIGN_TOP_LEFT, 30, 40);

    lv_label_set_text(lbl_city, current_city);
	update_city_label_style(current_city);  // Ez a függvény automatikusan beállítja a megfelelő stílust a városnév hosszától függően
    lv_label_set_text(lbl_temp, "");
    lv_label_set_text(lbl_hum, "Humidity: -");
    lv_label_set_text(lbl_wind, "Wind: -");    
    lv_label_set_text(lbl_uv, "UV index: -");
    lv_label_set_text(lbl_code, "Status");

    uint32_t period = g_cfg.weather.refresh_ms;
    if (period < 500) period = 500;
    weather_timer = lv_timer_create(update_weather_cb, period, NULL);

    lv_timer_ready(weather_timer);

    return scr;
}