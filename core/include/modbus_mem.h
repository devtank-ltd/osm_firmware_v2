#pragma once

#include <stdbool.h>

#include "base_types.h"


extern modbus_bus_t * modbus_bus;

extern void           modbus_print(cmd_output_t cmd_output);

extern char *         modbus_reg_type_get_str(modbus_reg_type_t type);

extern unsigned       modbus_get_device_count(void);
extern modbus_dev_t * modbus_get_device_by_id(unsigned unit_id);
extern modbus_dev_t * modbus_get_device_by_name(char * name);

extern modbus_dev_t * modbus_add_device(unsigned slave_id, char *name, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order);

extern bool           modbus_for_each_dev(bool (*cb)(modbus_dev_t * dev, void * userdata), void * userdata);
extern bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr);
extern modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name);
extern uint16_t       modbus_dev_get_unit_id(modbus_dev_t * dev);
extern bool           modbus_dev_for_each_reg(modbus_dev_t * dev, bool (*cb)(modbus_reg_t * reg, void * userdata), void * userdata);

extern void           modbus_dev_del(modbus_dev_t * dev);

extern modbus_reg_t * modbus_get_reg(char * name);

extern void           modbus_reg_del(modbus_reg_t * reg);

extern uint16_t          modbus_reg_get_unit_id(modbus_reg_t * reg);
extern bool              modbus_reg_get_name(modbus_reg_t * reg, char name[MODBUS_NAME_LEN + 1]);
extern modbus_reg_type_t modbus_reg_get_type(modbus_reg_t * reg);
extern modbus_dev_t    * modbus_reg_get_dev(modbus_reg_t * reg);
extern modbus_reg_state_t modbus_reg_get_state(modbus_reg_t * reg);
extern bool              modbus_reg_get_u16(modbus_reg_t * reg, uint16_t * value);
extern bool              modbus_reg_get_i16(modbus_reg_t * reg, int16_t * value);
extern bool              modbus_reg_get_u32(modbus_reg_t * reg, uint32_t * value);
extern bool              modbus_reg_get_i32(modbus_reg_t * reg, int32_t * value);
extern bool              modbus_reg_get_float(modbus_reg_t * reg, float * value);

extern bool             modbus_persist_config_cmp(modbus_bus_t* d0, modbus_bus_t* d1);

extern struct cmd_link_t* modbus_add_mem_commands(struct cmd_link_t* tail);
