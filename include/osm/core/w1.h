#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <osm/core/base_types.h>


bool     w1_reset(uint8_t index);
uint8_t  w1_read_byte(uint8_t index);
void     w1_send_byte(uint8_t index, uint8_t byte);
void     w1_init(uint8_t index);
void     w1_enable(unsigned io, bool enabled);
