#pragma once


#include <osm/core/measurements.h>


bool                         osm_bat_get_blocking(char* name, measurements_reading_t* value);
bool                         osm_bat_on_battery(bool* on_battery);

void                         osm_bat_inf_init(measurements_inf_t* inf);
struct cmd_link_t*           osm_bat_add_commands(struct cmd_link_t* tail);
