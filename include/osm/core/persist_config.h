#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/persist_config_header.h>


#define OSM_PERSIST_BASE_VERSION           4
#define OSM_PERSIST_VERSION_SET(_x)        ((OSM_PERSIST_BASE_VERSION << 8) | _x)


extern osm_persist_storage_t               persist_data __attribute__((aligned (16)));
extern osm_persist_measurements_storage_t  persist_measurements __attribute__((aligned (16)));

bool     osm_persistent_init(void);

void     osm_persist_commit();
void     osm_persistent_wipe(void);

void     osm_persist_set_fw_ready(uint32_t size);
void     osm_persist_set_log_debug_mask(uint32_t mask);
uint32_t osm_persist_get_log_debug_mask(void);
char*    osm_persist_get_serial_number(void);
char*    osm_persist_get_model(void);
char*    osm_persist_get_human_name(void);

struct osm_cmd_link_t* osm_persist_config_add_commands(struct osm_cmd_link_t* tail);
void osm_persist_config_inf_init(osm_measurements_inf_t* inf);

bool osm_persist_config_update(const osm_persist_storage_t* from_config, osm_persist_storage_t* to_config);
