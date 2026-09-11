#pragma once
#include <stdbool.h>
#include "config.h"

typedef struct {
    bool power;
    int set_temp;
    int room_temp;
} gree_status_t;

bool gree_power_set(const gree_dev_t *dev, bool on);
bool gree_temp_set(const gree_dev_t *dev, int temp);
bool gree_status_get(const gree_dev_t *dev, gree_status_t *out);

