#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"


extern bool     w1_reset(uint8_t index);
extern uint8_t  w1_read_byte(uint8_t index);
extern void     w1_send_byte(uint8_t index, uint8_t byte);
extern void     w1_init(uint8_t index);
