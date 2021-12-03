#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

typedef struct modbus_reg_t modbus_reg_t;

typedef void (*modbus_reg_cb)(modbus_reg_t * reg, uint8_t * data, uint8_t size);

#define MODBUS_READ_HOLDING_FUNC 3

#define modbus_debug(...) log_debug(DEBUG_MODBUS, "Modbus: " __VA_ARGS__)

struct modbus_reg_t
{
    char          name[16];
    uint16_t      reg_addr;
    uint8_t       reg_count;
    uint8_t       slave_id;
    uint8_t       func;
    modbus_reg_cb cb;
};

extern bool modbus_start_read(modbus_reg_t * reg);

extern void modbus_ring_process(ring_buf_t * ring);

extern void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing);

extern bool modbus_setup_from_str(char * str);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);

extern void modbus_init(void);
