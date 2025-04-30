#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <osm/core/ring.h>
#include <osm/core/log.h>
#include <osm/core/modbus_mem.h>


bool modbus_requires_echo_removal(void) __attribute__((weak));

uint16_t modbus_crc(uint8_t * buf, unsigned length);

bool modbus_set_reg(uint16_t unit_id, uint16_t reg_addr, uint8_t func, modbus_reg_type_t type, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order, float value);

bool modbus_start_read(modbus_reg_t * reg);

uint32_t modbus_start_delay(void);
uint32_t modbus_stop_delay(void);

bool modbus_has_pending(void);
bool modbus_get_reg_set_expected_done(bool * passfail);

void modbus_log(cmd_ctx_t * ctx);

void modbus_uart_ring_in_process(ring_buf_t * ring);
bool modbus_uart_ring_do_out_drain(ring_buf_t * ring);

void modbus_bus_init(modbus_bus_t * bus);
void modbus_init(void);

struct cmd_link_t* modbus_add_commands(struct cmd_link_t* tail);
