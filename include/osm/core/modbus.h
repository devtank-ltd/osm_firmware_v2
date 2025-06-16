#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <osm/core/ring.h>
#include <osm/core/log.h>
#include <osm/core/modbus_mem.h>


bool osm_modbus_requires_echo_removal(void) __attribute__((weak));

uint16_t osm_modbus_crc(uint8_t * buf, unsigned length);

bool osm_modbus_set_reg(uint16_t unit_id, uint16_t reg_addr, uint8_t func, modbus_reg_type_t type, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order, float value);

bool osm_modbus_start_read(modbus_reg_t * reg);

uint32_t osm_modbus_start_delay(void);
uint32_t osm_modbus_stop_delay(void);

bool osm_modbus_has_pending(void);
bool osm_modbus_get_reg_set_expected_done(bool * passfail);

void osm_modbus_log(cmd_ctx_t * ctx);

void osm_modbus_uart_ring_in_process(ring_buf_t * ring);
bool osm_modbus_uart_ring_do_out_drain(ring_buf_t * ring);

void osm_modbus_bus_init(modbus_bus_t * bus);
void osm_modbus_init(void);

struct cmd_link_t* osm_modbus_add_commands(struct cmd_link_t* tail);
