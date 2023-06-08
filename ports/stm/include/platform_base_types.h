#pragma once

#include <stdint.h>

#include <libopencm3/stm32/rcc.h>


#define PORT_TO_RCC(_port_)   (RCC_GPIOA + ((_port_ - GPIO_PORT_A_BASE) / 0x400))


typedef struct
{
    uint32_t              usart;
    enum rcc_periph_clken uart_clk;
    uint32_t              baud;
    uint8_t               databits:4;
    uint8_t               parity:2 /*osm_uart_parity_t*/;
    uint8_t               stop:2 /*osm_uart_stop_bits_t*/;
    uint32_t              gpioport;
    uint16_t              pins;
    uint8_t               alt_func_num;
    uint8_t               irqn;
    uint32_t              dma_addr;
    uint32_t              dma_unit;
    uint32_t              dma_rcc;
    uint8_t               dma_irqn;
    uint8_t               dma_channel;
    uint8_t               priority;
    uint8_t               enabled;
    uint8_t               dma_req;
} uart_channel_t;


typedef struct
{
    uint32_t port;
    uint32_t pins;
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
    uint32_t mem_addr;
} adc_setup_config_t;
