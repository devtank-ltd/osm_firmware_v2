#pragma once

#include <stdbool.h>

#include <osm/core/base_types.h>


extern osm_modbus_bus_t * modbus_bus;

void           osm_modbus_print(osm_cmd_ctx_t * ctx);

char *         osm_modbus_reg_type_get_str(osm_modbus_reg_type_t type);

unsigned       osm_modbus_get_device_count(void);
osm_modbus_dev_t * osm_modbus_get_device_by_id(unsigned unit_id);
osm_modbus_dev_t * osm_modbus_get_device_by_name(char * name);

osm_modbus_dev_t * osm_modbus_add_device(unsigned slave_id, char *name, osm_modbus_byte_orders_t byte_order, osm_modbus_word_orders_t word_order);

bool           osm_modbus_for_each_dev(bool (*cb)(osm_modbus_dev_t * dev, void * userdata), void * userdata);
bool           osm_modbus_dev_add_reg(osm_modbus_dev_t * dev, char * name, osm_modbus_reg_type_t type, uint8_t func, uint16_t reg_addr);
osm_modbus_reg_t * osm_modbus_dev_get_reg_by_name(osm_modbus_dev_t * dev, char * name);
uint16_t       osm_modbus_dev_get_unit_id(osm_modbus_dev_t * dev);
bool           osm_modbus_dev_for_each_reg(osm_modbus_dev_t * dev, bool (*cb)(osm_modbus_reg_t * reg, void * userdata), void * userdata);

void           osm_modbus_dev_del(osm_modbus_dev_t * dev);

osm_modbus_reg_t * osm_modbus_get_reg(char * name);

void           osm_modbus_reg_del(osm_modbus_reg_t * reg);

uint16_t          osm_modbus_reg_get_unit_id(osm_modbus_reg_t * reg);
bool              osm_modbus_reg_get_name(osm_modbus_reg_t * reg, char name[OSM_MODBUS_NAME_LEN + 1]);
osm_modbus_reg_type_t osm_modbus_reg_get_type(osm_modbus_reg_t * reg);
osm_modbus_dev_t    * modbus_reg_get_dev(osm_modbus_reg_t * reg);
osm_modbus_reg_state_t osm_modbus_reg_get_state(osm_modbus_reg_t * reg);
bool              osm_modbus_reg_get_u16(osm_modbus_reg_t * reg, uint16_t * value);
bool              osm_modbus_reg_get_i16(osm_modbus_reg_t * reg, int16_t * value);
bool              osm_modbus_reg_get_u32(osm_modbus_reg_t * reg, uint32_t * value);
bool              osm_modbus_reg_get_i32(osm_modbus_reg_t * reg, int32_t * value);
bool              osm_modbus_reg_get_float(osm_modbus_reg_t * reg, float * value);

bool             osm_modbus_persist_config_cmp(osm_modbus_bus_t* d0, osm_modbus_bus_t* d1);

struct osm_cmd_link_t* osm_modbus_add_mem_commands(struct osm_cmd_link_t* tail);
