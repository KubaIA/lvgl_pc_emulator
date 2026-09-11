#include "gree_backend.h"
#include <stdio.h>
#include <string.h>


static int stub_power[4] = {0};
static int stub_set_temp[4] = {24,24,24,24};
static int stub_room_temp[4] = {22,22,22,22};

static int find_index(const gree_dev_t *dev)
{
    for(int i = 0; i < 4; i++) {
        if(strcmp(g_cfg.gree.dev[i].ip, dev->ip) == 0)
            return i;
    }
    return 0;
}

bool gree_power_set(const gree_dev_t *dev, bool on)
{
    int idx = find_index(dev);
    stub_power[idx] = on ? 1 : 0;

    printf("[STUB] Power %s on %s (%s)\n",
           on ? "ON" : "OFF", dev->name, dev->ip);
    return true;
}

bool gree_temp_set(const gree_dev_t *dev, int temp)
{
    int idx = find_index(dev);
    stub_set_temp[idx] = temp;

    printf("[STUB] Set temp %d on %s (%s)\n",
           temp, dev->name, dev->ip);
    return true;
}

bool gree_status_get(const gree_dev_t *dev, gree_status_t *out)
{
    int idx = find_index(dev);

    out->power = stub_power[idx];
    out->set_temp = stub_set_temp[idx];
    out->room_temp = stub_room_temp[idx];

    return true;
}
