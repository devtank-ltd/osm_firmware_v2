#pragma once

#include <stdint.h>


typedef struct
{
    uint32_t              uart;
    uint32_t              baud;
    uint8_t               databits:4;
    uint8_t               parity:2 /*osm_uart_parity_t*/;
    uint8_t               stop:2 /*osm_uart_stop_bits_t*/;
    uint16_t              tx_pin;
    uint16_t              rx_pin;
    uint8_t               enabled;
    int                   pio; /* if pio < 0 then don't use pio */
} uart_channel_t;


typedef struct
{
    uint32_t pins;
} port_n_pins_t;
