#pragma once


#include <osm/core/measurements.h>


bool                         osm_bat_get_blocking(char* name, osm_measurements_reading_t* value);
bool                         osm_bat_on_battery(bool* on_battery);

void                         osm_bat_inf_init(osm_measurements_inf_t* inf);
struct osm_cmd_link_t*           osm_bat_add_commands(struct osm_cmd_link_t* tail);
