#pragma once

#include "platform_base_types.h"

#define SLEEP_MIN_SLEEP_TIME_MS                                     33

extern bool sleep_for_ms(uint32_t ms);
extern void sleep_exit_sleep_mode(void);
extern struct cmd_link_t* sleep_add_commands(struct cmd_link_t* tail);
