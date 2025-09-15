#pragma once

#include <stdbool.h>

bool osm_e_24lc00t_read(void* mem, unsigned len);
bool osm_e_24lc00t_write(void* mem, unsigned len);

#define osm_comms_eeprom_read       osm_e_24lc00t_read
#define osm_comms_eeprom_write      osm_e_24lc00t_write
