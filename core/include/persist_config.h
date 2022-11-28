#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "persist_config_header.h"


extern persist_storage_t               persist_data __attribute__((aligned (16)));
extern persist_measurements_storage_t  persist_measurements __attribute__((aligned (16)));

extern bool     persistent_init(void);

extern void     persist_commit();
extern void     persistent_wipe(void);

extern void     persist_set_fw_ready(uint32_t size);
extern void     persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);
extern char*    persist_get_serial_number(void);

extern struct cmd_link_t* persist_config_add_commands(struct cmd_link_t* tail);

