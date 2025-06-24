#pragma once

#include <stdint.h>

#include <osm/core/base_types.h>


osm_comms_type_t osm_comms_identify(void);
bool osm_comms_set_identity(void);

/* To be implemented by caller.*/
bool osm_comms_eeprom_read(void* mem, unsigned len) __attribute__((weak));
bool osm_comms_eeprom_write(void* mem, unsigned len) __attribute__((weak));

struct osm_cmd_link_t* osm_comms_identify_add_commands(struct osm_cmd_link_t* tail);
