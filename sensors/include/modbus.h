#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"
#include "pinmap.h"
#include "log.h"
#include "modbus_mem.h"


extern uint16_t modbus_crc(uint8_t * buf, unsigned length);


extern bool modbus_start_read(modbus_reg_t * reg);

extern void modbus_ring_process(ring_buf_t * ring);

extern void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing);

extern bool modbus_setup_from_str(char * str);
extern bool modbus_add_dev_from_str(char* str);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);

extern void  modbus_log();

extern void modbus_init(void);
