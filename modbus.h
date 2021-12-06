#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

#define MODBUS_READ_HOLDING_FUNC 3

#define MODBUS_NAME_LEN 4
#define MODBUS_DEV_REGS 4

#define modbus_debug(...) log_debug(DEBUG_MODBUS, "Modbus: " __VA_ARGS__)

typedef enum
{
    MODBUS_REG_TYPE_INVALID,
    MODBUS_REG_TYPE_U16,
    MODBUS_REG_TYPE_U32,
    MODBUS_REG_TYPE_FLOAT,
    MODBUS_REG_TYPE_MAX = MODBUS_REG_TYPE_FLOAT,
} modbus_reg_type_t;

typedef struct modbus_dev_t modbus_dev_t;
typedef struct modbus_reg_t modbus_reg_t;


extern char * modbus_reg_type_get_str(modbus_reg_type_t type);


extern bool modbus_start_read(modbus_reg_t * reg);

extern void modbus_ring_process(ring_buf_t * ring);

extern void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing);

extern bool modbus_setup_from_str(char * str);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);


extern void           modbus_log();

extern unsigned       modbus_get_device_count(void);
extern modbus_dev_t * modbus_get_device(unsigned index);
extern modbus_dev_t * modbus_get_device_by_id(unsigned slave_id);

extern modbus_dev_t * modbus_add_device(unsigned slave_id, char *name);

extern void           modbus_config_wipe(void);

extern bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr);
extern bool           modbus_dev_is_enabled(modbus_dev_t * dev);
extern unsigned       modbus_get_reg_num(modbus_dev_t * dev);
extern modbus_reg_t * modbus_dev_get_reg(modbus_dev_t * dev, unsigned index);
extern modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name);
extern char *         modbus_dev_get_name(modbus_dev_t * dev);
extern uint16_t       modbus_dev_get_slave_id(modbus_dev_t * dev);

extern modbus_reg_t * modbus_get_reg(char * name);

extern modbus_reg_type_t modbus_reg_get_type(modbus_reg_t * reg);
extern modbus_dev_t    * modbus_reg_get_dev(modbus_reg_t * reg);
extern bool              modbus_reg_get_u16(modbus_reg_t * reg, uint16_t * value);
extern bool              modbus_reg_get_u32(modbus_reg_t * reg, uint32_t * value);
extern bool              modbus_reg_get_float(modbus_reg_t * reg, float * value);

extern void modbus_save();

extern void modbus_init(void);
