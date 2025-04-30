#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <osm/core/ring.h>
#include <osm/core/log.h>
#include <osm/core/modbus_mem.h>


extern bool modbus_requires_echo_removal(void) __attribute__((weak));

extern uint16_t modbus_crc(uint8_t * buf, unsigned length);

extern bool modbus_set_reg(uint16_t unit_id, uint16_t reg_addr, uint8_t func, modbus_reg_type_t type, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order, float value);

extern bool modbus_start_read(modbus_reg_t * reg);

extern uint32_t modbus_start_delay(void);
extern uint32_t modbus_stop_delay(void);

extern bool modbus_has_pending(void);
extern bool modbus_get_reg_set_expected_done(bool * passfail);

extern void modbus_log(cmd_ctx_t * ctx);

extern void modbus_uart_ring_in_process(ring_buf_t * ring);
extern bool modbus_uart_ring_do_out_drain(ring_buf_t * ring);

extern void modbus_bus_init(modbus_bus_t * bus);
extern void modbus_init(void);

extern struct cmd_link_t* modbus_add_commands(struct cmd_link_t* tail);
