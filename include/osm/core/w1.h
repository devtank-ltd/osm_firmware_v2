#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <osm/core/base_types.h>


bool     osm_w1_reset(uint8_t index);
uint8_t  osm_w1_read_byte(uint8_t index);
void     osm_w1_send_byte(uint8_t index, uint8_t byte);
void     osm_w1_init(uint8_t index);
void     osm_w1_enable(unsigned io, bool enabled);
