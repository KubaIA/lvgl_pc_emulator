#include "gree_backend.h"
#include "cJSON.h"
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

static bool gree_udp_send_recv(const char *ip, const char *json_out, char *json_in, int json_in_size)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock < 0) return false;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7000);
    dest.sin_addr.s_addr = inet_addr(ip);

    sendto(sock, json_out, strlen(json_out), 0, (struct sockaddr*)&dest, sizeof(dest));

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int len = recv(sock, json_in, json_in_size - 1, 0);
    close(sock);

    if(len <= 0) return false;

    json_in[len] = '\0';
    return true;
}

bool gree_power_set(const gree_dev_t *dev, bool on)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "t", "cmd");
    cJSON_AddStringToObject(root, "mac", dev->mac);

    cJSON *opt = cJSON_CreateArray();
    cJSON_AddItemToArray(opt, cJSON_CreateString("Pow"));
    cJSON_AddItemToObject(root, "opt", opt);

    cJSON *p = cJSON_CreateArray();
    cJSON_AddItemToArray(p, cJSON_CreateNumber(on ? 1 : 0));
    cJSON_AddItemToObject(root, "p", p);

    char *json_out = cJSON_PrintUnformatted(root);
    char json_in[512];

    bool ok = gree_udp_send_recv(dev->ip, json_out, json_in, sizeof(json_in));

    cJSON_free(json_out);
    cJSON_Delete(root);

    return ok;
}

bool gree_temp_set(const gree_dev_t *dev, int temp)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "t", "cmd");
    cJSON_AddStringToObject(root, "mac", dev->mac);

    cJSON *opt = cJSON_CreateArray();
    cJSON_AddItemToArray(opt, cJSON_CreateString("SetTem"));
    cJSON_AddItemToObject(root, "opt", opt);

    cJSON *p = cJSON_CreateArray();
    cJSON_AddItemToArray(p, cJSON_CreateNumber(temp));
    cJSON_AddItemToObject(root, "p", p);

    char *json_out = cJSON_PrintUnformatted(root);
    char json_in[512];

    bool ok = gree_udp_send_recv(dev->ip, json_out, json_in, sizeof(json_in));

    cJSON_free(json_out);
    cJSON_Delete(root);

    return ok;
}

bool gree_status_get(const gree_dev_t *dev, gree_status_t *out)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "t", "status");
    cJSON_AddStringToObject(root, "mac", dev->mac);

    cJSON *cols = cJSON_CreateArray();
    cJSON_AddItemToArray(cols, cJSON_CreateString("Pow"));
    cJSON_AddItemToArray(cols, cJSON_CreateString("SetTem"));
    cJSON_AddItemToArray(cols, cJSON_CreateString("TemSen"));
    cJSON_AddItemToObject(root, "cols", cols);

    char *json_out = cJSON_PrintUnformatted(root);
    char json_in[512];

    bool ok = gree_udp_send_recv(dev->ip, json_out, json_in, sizeof(json_in));

    cJSON_free(json_out);
    cJSON_Delete(root);

    if(!ok) return false;

    cJSON *resp = cJSON_Parse(json_in);
    if(!resp) return false;

    cJSON *dat = cJSON_GetObjectItem(resp, "dat");
    if(!cJSON_IsArray(dat)) {
        cJSON_Delete(resp);
        return false;
    }

    out->power     = cJSON_GetArrayItem(dat, 0)->valueint == 1;
    out->set_temp  = cJSON_GetArrayItem(dat, 1)->valueint;
    out->room_temp = cJSON_GetArrayItem(dat, 2)->valueint - 40;

    cJSON_Delete(resp);
    return true;
}
