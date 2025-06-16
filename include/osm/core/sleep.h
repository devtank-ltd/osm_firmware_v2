#pragma once

#include "platform_base_types.h"

#define SLEEP_MIN_SLEEP_TIME_MS                                     33

bool osm_sleep_for_ms(uint32_t ms);
void osm_sleep_exit_sleep_mode(void);
struct cmd_link_t* osm_sleep_add_commands(struct cmd_link_t* tail);
