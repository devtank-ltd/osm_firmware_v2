#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"


typedef struct
{
    port_n_pins_t       port_n_pins;
} w1_instance_t;


extern bool     w1_reset(w1_instance_t* instance);
extern uint8_t  w1_read_byte(w1_instance_t* instance);
extern void     w1_send_byte(w1_instance_t* instance, uint8_t byte);
