#pragma once

#include <stdint.h>

#include "base_types.h"


comms_type_t comms_identify(void);
bool comms_set_identity(void);

/* To be implemented by caller.*/
bool comms_eeprom_read(void* mem, unsigned len) __attribute__((weak));
bool comms_eeprom_write(void* mem, unsigned len) __attribute__((weak));

struct cmd_link_t* comms_identify_add_commands(struct cmd_link_t* tail);
