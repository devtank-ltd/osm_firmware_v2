#pragma once


#include <osm/core/measurements.h>


bool                         bat_get_blocking(char* name, measurements_reading_t* value);
bool                         bat_on_battery(bool* on_battery);

void                         bat_inf_init(measurements_inf_t* inf);
struct cmd_link_t*           bat_add_commands(struct cmd_link_t* tail);
