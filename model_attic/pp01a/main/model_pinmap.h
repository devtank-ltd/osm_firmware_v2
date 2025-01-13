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
    {                                                               \
        .uart = 1,        /* COMMS */                               \
        .baud = UART_1_SPEED,                                       \
        .databits = UART_1_DATABITS,                                \
        .parity = UART_1_PARITY,                                    \
        .stop = UART_1_STOP,                                        \
        .tx_pin = 4,                                                \
        .rx_pin = 5,                                                \
        .enabled = true,                                            \
    }                                                               \
}

#define UART_CHANNELS_COUNT 2

#define CMD_UART 0
#define COMMS_UART 1

#define UART_BUFFERS_INIT                \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];  \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];\
char uart_1_in_buf[UART_1_IN_BUF_SIZE];  \
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];

#define UART_IN_RINGS                                   \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),\
}

#define UART_OUT_RINGS                                    \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),\
}

