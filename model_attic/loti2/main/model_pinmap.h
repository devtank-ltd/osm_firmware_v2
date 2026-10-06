#pragma once

#define UART_CHANNELS                                               \
{                                                                   \
    {                                                               \
        .uart = 0,        /* DEBUG */                               \
        .baud = UART_0_SPEED,                                       \
        .databits = UART_0_DATABITS,                                \
        .parity = UART_0_PARITY,                                    \
        .stop = UART_0_STOP,                                        \
        .tx_pin = 0,                                                \
        .rx_pin = 1,                                                \
        .enabled = true,                                            \
    },                                                              \
}

#define OSM_UART_CHANNELS_COUNT 1

#define CMD_UART            0

#define OSM_UART_BUFFERS_INIT            \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];  \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];

#define OSM_UART_IN_RINGS                               \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
}

#define OSM_UART_OUT_RINGS                                \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
}
