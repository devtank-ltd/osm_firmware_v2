#pragma once

#include <stdbool.h>

#include "pinmap.h"
extern void           modbus_log();

extern modbus_reg_type_t modbus_reg_type_from_str(const char * type, const char ** pos);
extern char *         modbus_reg_type_get_str(modbus_reg_type_t type);

extern unsigned       modbus_get_device_count(void);
extern modbus_dev_t * modbus_get_device(unsigned index);
extern modbus_dev_t * modbus_get_device_by_id(unsigned slave_id);
extern modbus_dev_t * modbus_get_device_by_name(char * name);

extern modbus_dev_t * modbus_add_device(unsigned slave_id, char *name, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order);

extern bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr);
extern unsigned       modbus_dev_get_reg_num(modbus_dev_t * dev);
extern modbus_reg_t * modbus_dev_get_reg(modbus_dev_t * dev, unsigned index);
extern modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name);
extern uint16_t       modbus_dev_get_slave_id(modbus_dev_t * dev);

extern void           modbus_dev_del(modbus_dev_t * dev);

extern modbus_reg_t * modbus_get_reg(char * name);

extern void           modbus_reg_del(modbus_reg_t * reg);

extern bool              modbus_reg_get_name(modbus_reg_t * reg, char name[MODBUS_NAME_LEN + 1]);
extern modbus_reg_type_t modbus_reg_get_type(modbus_reg_t * reg);
extern modbus_dev_t    * modbus_reg_get_dev(modbus_reg_t * reg);
extern bool              modbus_reg_get_u16(modbus_reg_t * reg, uint16_t * value);
extern bool              modbus_reg_get_u32(modbus_reg_t * reg, uint32_t * value);
extern bool              modbus_reg_get_float(modbus_reg_t * reg, float * value);
