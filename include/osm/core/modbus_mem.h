#pragma once

#include <stdbool.h>

#include <osm/core/base_types.h>


extern modbus_bus_t * modbus_bus;

void           modbus_print(cmd_ctx_t * ctx);

char *         modbus_reg_type_get_str(modbus_reg_type_t type);

unsigned       modbus_get_device_count(void);
modbus_dev_t * modbus_get_device_by_id(unsigned unit_id);
modbus_dev_t * modbus_get_device_by_name(char * name);

modbus_dev_t * modbus_add_device(unsigned slave_id, char *name, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order);

bool           modbus_for_each_dev(bool (*cb)(modbus_dev_t * dev, void * userdata), void * userdata);
bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr);
modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name);
uint16_t       modbus_dev_get_unit_id(modbus_dev_t * dev);
bool           modbus_dev_for_each_reg(modbus_dev_t * dev, bool (*cb)(modbus_reg_t * reg, void * userdata), void * userdata);

void           modbus_dev_del(modbus_dev_t * dev);

modbus_reg_t * modbus_get_reg(char * name);

void           modbus_reg_del(modbus_reg_t * reg);

uint16_t          modbus_reg_get_unit_id(modbus_reg_t * reg);
bool              modbus_reg_get_name(modbus_reg_t * reg, char name[MODBUS_NAME_LEN + 1]);
modbus_reg_type_t modbus_reg_get_type(modbus_reg_t * reg);
modbus_dev_t    * modbus_reg_get_dev(modbus_reg_t * reg);
modbus_reg_state_t modbus_reg_get_state(modbus_reg_t * reg);
bool              modbus_reg_get_u16(modbus_reg_t * reg, uint16_t * value);
bool              modbus_reg_get_i16(modbus_reg_t * reg, int16_t * value);
bool              modbus_reg_get_u32(modbus_reg_t * reg, uint32_t * value);
bool              modbus_reg_get_i32(modbus_reg_t * reg, int32_t * value);
bool              modbus_reg_get_float(modbus_reg_t * reg, float * value);

bool             modbus_persist_config_cmp(modbus_bus_t* d0, modbus_bus_t* d1);

struct cmd_link_t* modbus_add_mem_commands(struct cmd_link_t* tail);
