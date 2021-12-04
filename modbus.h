#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

#define MODBUS_READ_HOLDING_FUNC 3

#define MODBUS_NAME_LEN 8
#define MODBUS_DEV_REGS 8

#define modbus_debug(...) log_debug(DEBUG_MODBUS, "Modbus: " __VA_ARGS__)

typedef enum
{
    MODBUS_REG_TYPE_BIN,
    MODBUS_REG_TYPE_U16_ID,
    MODBUS_REG_TYPE_U16,
    MODBUS_REG_TYPE_U32,
    MODBUS_REG_TYPE_FLOAT,
} modbus_reg_type_t;

typedef struct modbus_dev_t modbus_dev_t;

typedef struct
{
    char              name[MODBUS_NAME_LEN];
    uint16_t          _; /* pad */
    uint16_t          reg_addr;
    uint8_t           slave_id;
    uint8_t           type; /*modbus_reg_type_t*/
    uint8_t           reg_count;
    uint8_t           func;
} __attribute__((__packed__)) modbus_reg_t;

struct __attribute__((__packed__)) modbus_dev_t
{
    char           name[MODBUS_NAME_LEN];
    modbus_reg_t * regs[MODBUS_DEV_REGS];
    uint8_t        enabled;
    uint8_t        slave_id;
    uint8_t        reg_num;
    uint8_t        _; /* pad */
};

extern bool modbus_start_read(modbus_reg_t * reg);

extern void modbus_ring_process(ring_buf_t * ring);

extern void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing);

extern bool modbus_setup_from_str(char * str);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);

extern void modbus_init(void);


extern unsigned       modbus_get_device_count(void);
extern modbus_dev_t * modbus_get_device(unsigned index);
extern modbus_dev_t * modbus_get_device_by_id(unsigned slave_id);

extern modbus_dev_t * modbus_add_device(unsigned slave_id, char *name);
extern bool           modbus_dev_add_reg(modbus_dev_t * dev, modbus_reg_t * reg);

extern modbus_reg_t * modbus_dev_get_reg(modbus_dev_t * dev, char * name);
extern modbus_reg_t * modbus_get_reg(char * name);


typedef struct
{
    modbus_reg_t base;
    uint32_t     ref_count;
    uint8_t    * ref;
} __attribute__((__packed__)) modbus_reg_bin_check_t;

typedef struct
{
    modbus_reg_t base;
    uint32_t     ids_count;
    uint16_t   * ids;
} __attribute__((__packed__)) modbus_reg_ids_check_t;

typedef struct
{
    modbus_reg_t base;
    uint16_t     value;
    uint16_t     valid;
} __attribute__((__packed__)) modbus_reg_u16_t;

typedef struct
{
    modbus_reg_t base;
    uint32_t     value;
    uint32_t     valid;
} __attribute__((__packed__)) modbus_reg_u32_t;

typedef struct
{
    modbus_reg_t base;
    float        value;
    uint32_t     valid;
} __attribute__((__packed__)) modbus_reg_float_t;

extern void modbus_save(void);
