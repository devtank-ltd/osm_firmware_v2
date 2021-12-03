#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

typedef struct modbus_reg_t modbus_reg_t;

typedef void (*modbus_reg_cb)(modbus_reg_t * reg, uint8_t * data, uint8_t size);

#define MODBUS_READ_HOLDING_FUNC 3

#define modbus_debug(...) log_debug(DEBUG_MODBUS, "Modbus: " __VA_ARGS__)

typedef struct
{
    bool     enabled;
    uint8_t  slave_id;
} modbus_dev_t;


struct modbus_reg_t
{
    char          name[16];
    modbus_dev_t *dev;
    modbus_reg_cb cb;
    uint16_t      reg_addr;
    uint8_t       reg_count;
    uint8_t       func;
};

extern bool modbus_start_read(modbus_reg_t * reg);

extern void modbus_ring_process(ring_buf_t * ring);

extern void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing);

extern bool modbus_setup_from_str(char * str);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);

extern void modbus_init(void);


typedef struct
{
    modbus_reg_t base;
    uint8_t ref_count;
    uint8_t ref[1]; /* At least 1 byte */
} modbus_reg_bin_check_t;

/* Binary content must match of device is disabled. */
extern void modbus_reg_bin_check_cb(modbus_reg_bin_check_t * reg, uint8_t * data, uint8_t size);

typedef struct
{
    modbus_reg_t base;
    uint8_t ids_count;
    uint16_t ids[1]; /* At least one 16bit value*/
} modbus_reg_ids_check_t;

/* Given register value must match of one of the IDs or dev is disabled. */
extern void modbus_reg_ids_check_cb(modbus_reg_ids_check_t * reg, uint8_t * data, uint8_t size);
