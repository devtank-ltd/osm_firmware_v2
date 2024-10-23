#pragma once

#include <stdbool.h>

bool e_24lc00t_read(void* mem, unsigned len);
bool e_24lc00t_write(void* mem, unsigned len);

#define comms_eeprom_read       e_24lc00t_read
#define comms_eeprom_write      e_24lc00t_write
