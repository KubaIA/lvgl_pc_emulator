#include "time_service.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================
   ESP32 PLATFORM (SNTP)
   ============================================================ */
#ifdef ESP_PLATFORM
#include "esp_sntp.h"

void time_service_init(void)
{
    // Időzóna beállítása (CET/CEST)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    // NTP szerverek
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_init();
}

bool time_service_sync(void)
{
    // Várunk, amíg az SNTP beállítja az időt
    for (int i = 0; i < 20; i++) {
        time_t now;
        time(&now);
        if (now > 100000) return true;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    return false;
}

bool time_service_get_local(struct tm* out)
{
    time_t now;
    time(&now);
    if (now < 100000) return false;

    localtime_r(&now, out);
    return true;
}

#endif // ESP_PLATFORM



/* ============================================================
   PC PLATFORM (Windows + MinGW + CMake)
   ============================================================ */
#ifndef ESP_PLATFORM

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

   /* -----------------------------
      NTP lekérdezés (UTC)
      ----------------------------- */
static bool ntp_query_utc(time_t* out)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(123);

    // time.nist.gov (129.6.15.28)
    inet_pton(AF_INET, "129.6.15.28", &server.sin_addr);

    unsigned char packet[48] = { 0 };
    packet[0] = 0b11100011; // LI, Version, Mode

    int sent = sendto(sock, (const char*)packet, 48, 0,
        (struct sockaddr*)&server, sizeof(server));
    if (sent < 0) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    struct sockaddr_in from;
    int fromlen = sizeof(from);
    int rec = recvfrom(sock, (char*)packet, 48, 0,
        (struct sockaddr*)&from, &fromlen);

    closesocket(sock);
    WSACleanup();

    if (rec < 0) return false;

    // NTP timestamp (1900 óta eltelt másodpercek)
    unsigned long secs_since_1900 =
        (packet[40] << 24) | (packet[41] << 16) |
        (packet[42] << 8) | (packet[43]);

    // 1900 → 1970 átszámítás
    const unsigned long seventy_years = 2208988800UL;
    *out = secs_since_1900 - seventy_years;

    return true;
}

const char* time_service_get_timezone(void)
{
    const char* tz = getenv("TZ");
    return tz ? tz : "UNKNOWN";
}

/* -----------------------------
   INIT (PC)
   ----------------------------- */
void time_service_init(void)
{
    // Windows alatt nincs setenv(), helyette _putenv_s()
    _putenv_s("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3");
    tzset();
}

/* -----------------------------
   SYNC (PC)
   ----------------------------- */
bool time_service_sync(void)
{
    time_t now;
    if (ntp_query_utc(&now)) {

        // A PC-s "belső időt" frissítjük
        // (nem a Windows rendszerórát!)
        _time64(&now);

        return true;
    }
    return false;
}

/* -----------------------------
   GET LOCAL TIME (PC)
   ----------------------------- */
bool time_service_get_local(struct tm* out)
{
    time_t now;
    time(&now);

    if (now < 100000) return false;

    localtime_s(out, &now);
    return true;
}

#endif // NOT ESP_PLATFORM
