#pragma once

#include <stdint.h>


typedef struct
{
    uint32_t              baud;
    uint8_t               databits:4;
    uint8_t               parity:2 /*osm_uart_parity_t*/;
    uint8_t               stop:2 /*osm_uart_stop_bits_t*/;
    uint8_t               enabled;
    uint32_t              fd;
} uart_channel_t;


typedef struct
{
    uint32_t index;
} port_n_pins_t;


typedef struct
{
    uint32_t rcc;
    uint32_t i2c;
    uint32_t speed;
    uint32_t clock_megahz;
    uint32_t gpio_func;
    port_n_pins_t port_n_pins;
} i2c_def_t;


typedef struct
{
    uintptr_t mem_addr;
} adc_setup_config_t;
