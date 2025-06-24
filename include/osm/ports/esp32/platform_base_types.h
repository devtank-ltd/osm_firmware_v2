#pragma once

#include <driver/uart.h>
#include <driver/i2c.h>


typedef struct
{
    bool                  enabled;
    uint32_t              uart;
    uint32_t              tx_pin;
    uint32_t              rx_pin;
    uart_config_t         config;
} osm_uart_channel_t;

/* No port on this platform */
#define osm_port_n_pins_t uint32_t

typedef struct
{
    int          port; 
    i2c_config_t config;
} osm_i2c_def_t;


typedef struct
{
    uint32_t mem_addr;
} osm_adc_setup_config_t;
