#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

typedef struct modbus_reg_t modbus_reg_t;

typedef void modbus_reg_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size);


struct modbus_reg_t
{
    char name[16];
    uint16_t addr;
    uint8_t  len;
    uint8_t  slave_id;
};

extern bool modbus_start_read_holding(modbus_reg_t * reg, modbus_reg_cb cb);

extern void modbus_ring_process(ring_buf_t * ring);
