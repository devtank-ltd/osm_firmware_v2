#pragma once
#include <stdbool.h>
#include <stdint.h>


extern bool     w1_reset(uint8_t w1_port, uint8_t w1_pin);
extern uint8_t  w1_read_byte(uint8_t w1_port, uint8_t w1_pin);
extern void     w1_send_byte(uint8_t w1_port, uint8_t w1_pin, uint8_t byte);
