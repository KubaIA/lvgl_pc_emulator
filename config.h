#pragma once
#include <stdint.h>

#define GREE_MAX 4

typedef struct {
    char city[64];        // <-- ÚJ: városnév
    char timezone[64];    // <--- ÚJ: pl. "Europe/Budapest"
    int32_t utc_offset_sec; // <--- EZ KELL AZ ÓRÁNAK (pl. 3600 vagy 7200)
    uint32_t refresh_ms;  // marad
} weather_cfg_t;

typedef struct {
    char ip[64];
    uint32_t refresh_ms;
} inverter_cfg_t;

typedef struct {
    char name[32];
    char ip[64];
} gree_dev_t;

typedef struct {
    char ssid[64];
    char password[64];
} wifi_cfg_t;

typedef struct {
    uint32_t refresh_ms;
    gree_dev_t dev[GREE_MAX];
} gree_cfg_t;

typedef struct {
    wifi_cfg_t wifi;
    weather_cfg_t weather;
    inverter_cfg_t inverter;
    gree_cfg_t gree;
} app_config_t;

extern app_config_t g_cfg;

void config_set_defaults(app_config_t * cfg);
int  config_load_json(app_config_t * cfg, const char * path);
int config_save_json(const app_config_t* cfg, const char* path);

void strip_accents(char* str);