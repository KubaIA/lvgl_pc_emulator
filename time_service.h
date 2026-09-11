#pragma once
#include <time.h>
#include <stdbool.h>

void time_service_init(void);
bool time_service_sync(void);
bool time_service_get_local(struct tm* out);

const char* time_service_get_timezone(void);
