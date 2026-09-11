#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gree_backend.h"

/* Globális konfigurációs objektum */
app_config_t g_cfg;

/* ---- Alapértelmezett értékek beállítása ---- */
void config_set_defaults(app_config_t* cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    strcpy(cfg->weather.city, "Budapest");
    cfg->weather.refresh_ms = 5000;
    cfg->weather.utc_offset_sec = 3600; // Legyen egy alapértelmezett +1 óra

    strcpy(cfg->inverter.ip, "192.168.1.133");
    cfg->inverter.refresh_ms = 2000;

    cfg->gree.refresh_ms = 3000;
    strcpy(cfg->gree.dev[0].name, "AC1");
    strcpy(cfg->gree.dev[0].ip, "192.168.1.124");
    strcpy(cfg->gree.dev[1].name, "AC2");
    strcpy(cfg->gree.dev[1].ip, "192.168.1.127");
    strcpy(cfg->gree.dev[2].name, "AC3");
    strcpy(cfg->gree.dev[2].ip, "192.168.1.126");
}

/* ---- Segédfüggvény: Teljes fájl beolvasása memóriába ---- */
static char* read_all(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }

    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* ---- JSON segédfüggvények (Getterek) ---- */
static void get_str(cJSON* obj, const char* key, char* out, size_t out_sz)
{
    cJSON* it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(it) && it->valuestring) {
        strncpy(out, it->valuestring, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}

static void get_u32(cJSON* obj, const char* key, uint32_t* out)
{
    cJSON* it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(it)) *out = (uint32_t)(it->valuedouble + 0.5);
}

/* ---- KONFIGURÁCIÓ BETÖLTÉSE ---- */
int config_load_json(app_config_t* cfg, const char* path)
{
    char* txt = read_all(path);
    if (!txt) return 0;

    cJSON* root = cJSON_Parse(txt);
    free(txt);
    if (!root) return 0;

    /* GREE szekció ürítése beolvasás előtt */
    memset(&cfg->gree, 0, sizeof(cfg->gree));

    // Weather
    cJSON* weather = cJSON_GetObjectItem(root, "weather");
    if (cJSON_IsObject(weather)) {
        get_str(weather, "city", cfg->weather.city, sizeof(cfg->weather.city));
        get_u32(weather, "refresh_ms", &cfg->weather.refresh_ms);
    }

    // Inverter
    cJSON* inv = cJSON_GetObjectItem(root, "inverter");
    if (cJSON_IsObject(inv)) {
        get_str(inv, "ip", cfg->inverter.ip, sizeof(cfg->inverter.ip));
        get_u32(inv, "refresh_ms", &cfg->inverter.refresh_ms);
    }

    // Gree
    cJSON* gree = cJSON_GetObjectItem(root, "gree");
    if (cJSON_IsObject(gree)) {
        get_u32(gree, "refresh_ms", &cfg->gree.refresh_ms);

        cJSON* devs = cJSON_GetObjectItem(gree, "devices");
        if (cJSON_IsArray(devs)) {
            int n = cJSON_GetArraySize(devs);
            if (n > GREE_MAX) n = GREE_MAX;

            for (int i = 0; i < n; i++) {
                cJSON* d = cJSON_GetArrayItem(devs, i);
                if (!cJSON_IsObject(d)) continue;
                get_str(d, "name", cfg->gree.dev[i].name, sizeof(cfg->gree.dev[i].name));
                get_str(d, "ip", cfg->gree.dev[i].ip, sizeof(cfg->gree.dev[i].ip));
            }
        }
    }

    cJSON_Delete(root);
    return 1;
}

/* ---- KONFIGURÁCIÓ MENTÉSE (ÚJ) ---- */
int config_save_json(const app_config_t* cfg, const char* path)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) return 0;

    // Weather szekció
    cJSON* weather = cJSON_CreateObject();
    cJSON_AddStringToObject(weather, "city", cfg->weather.city);
    cJSON_AddStringToObject(weather, "timezone", cfg->weather.timezone);
    cJSON_AddNumberToObject(weather, "utc_offset_sec", cfg->weather.utc_offset_sec);
    cJSON_AddNumberToObject(weather, "refresh_ms", cfg->weather.refresh_ms);
    cJSON_AddItemToObject(root, "weather", weather);

    // Inverter szekció
    cJSON* inv = cJSON_CreateObject();
    cJSON_AddStringToObject(inv, "ip", cfg->inverter.ip);
    cJSON_AddNumberToObject(inv, "refresh_ms", cfg->inverter.refresh_ms);
    cJSON_AddItemToObject(root, "inverter", inv);

    // Gree szekció
    cJSON* gree = cJSON_CreateObject();
    cJSON_AddNumberToObject(gree, "refresh_ms", cfg->gree.refresh_ms);

    cJSON* devs = cJSON_CreateArray();
    for (int i = 0; i < GREE_MAX; i++) {
        // Csak akkor mentjük, ha van megadva IP cím
        if (cfg->gree.dev[i].ip[0] != '\0') {
            cJSON* d = cJSON_CreateObject();
            cJSON_AddStringToObject(d, "name", cfg->gree.dev[i].name);
            cJSON_AddStringToObject(d, "ip", cfg->gree.dev[i].ip);
            cJSON_AddItemToArray(devs, d);
        }
    }
    cJSON_AddItemToObject(gree, "devices", devs);
    cJSON_AddItemToObject(root, "gree", gree);

    // JSON string generálása (szép formázással)
    char* txt = cJSON_Print(root);
    if (!txt) {
        cJSON_Delete(root);
        return 0;
    }

    // Fájlba írás
    FILE* f = fopen(path, "w");
    if (!f) {
        free(txt);
        cJSON_Delete(root);
        return 0;
    }

    fputs(txt, f);
    fclose(f);

    free(txt);
    cJSON_Delete(root);
    return 1;
}

void strip_accents(char* str) {
    if (str == NULL) return;
    for (int i = 0; str[i]; i++) {
        unsigned char c1 = (unsigned char)str[i];
        if (c1 == 0xC3) {
            unsigned char c2 = (unsigned char)str[i + 1];
            bool found = true;
            switch (c2) {
            case 0x81: str[i] = 'A'; break; // Á
            case 0x89: str[i] = 'E'; break; // É
            case 0x8D: str[i] = 'I'; break; // Í
            case 0x93: str[i] = 'O'; break; // Ó
            case 0x96: str[i] = 'O'; break; // Ö
            case 0x9A: str[i] = 'U'; break; // Ú
            case 0x9C: str[i] = 'U'; break; // Ü
            case 0xA1: str[i] = 'a'; break; // á
            case 0xA9: str[i] = 'e'; break; // é
            case 0xAD: str[i] = 'i'; break; // í
            case 0xB3: str[i] = 'o'; break; // ó
            case 0xB6: str[i] = 'o'; break; // ö
            case 0xBA: str[i] = 'u'; break; // ú
            case 0xBC: str[i] = 'u'; break; // ü
            default: found = false; break;
            }
            if (found) memmove(&str[i + 1], &str[i + 2], strlen(&str[i + 2]) + 1);
        }
        else if (c1 == 0xC5) {
            unsigned char c2 = (unsigned char)str[i + 1];
            bool found = true;
            switch (c2) {
            case 0x90: str[i] = 'O'; break; // Ő
            case 0x91: str[i] = 'o'; break; // ő
            case 0xB0: str[i] = 'U'; break; // Ű
            case 0xB1: str[i] = 'u'; break; // ű
            default: found = false; break;
            }
            if (found) memmove(&str[i + 1], &str[i + 2], strlen(&str[i + 2]) + 1);
        }
    }
}