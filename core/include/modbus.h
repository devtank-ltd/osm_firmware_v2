#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ring.h"
#include "log.h"
#include "modbus_mem.h"


extern uint16_t modbus_crc(uint8_t * buf, unsigned length);

extern bool modbus_start_read(modbus_reg_t * reg);

extern void modbus_setup(unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop, bool binary_framing);

extern bool modbus_setup_from_str(char * str);
extern bool modbus_add_dev_from_str(char* str);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);

extern bool modbus_has_pending(void);

extern void modbus_log();

extern void modbus_uart_ring_in_process(ring_buf_t * ring);
extern bool modbus_uart_ring_do_out_drain(ring_buf_t * ring);

extern void modbus_bus_init(modbus_bus_t * bus);
extern void modbus_init(void);

extern struct cmd_link_t* modbus_add_commands(struct cmd_link_t* tail);
