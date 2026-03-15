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
        .pio = -1,                                                  \
    },                                                              \
    {                                                               \
        .uart = 1,        /* COMMS */                               \
        .baud = UART_1_SPEED,                                       \
        .databits = UART_1_DATABITS,                                \
        .parity = UART_1_PARITY,                                    \
        .stop = UART_1_STOP,                                        \
        .tx_pin = 2,                                                \
        .rx_pin = 3,                                                \
        .enabled = true,                                            \
        .pio = 0,                                                   \
    },                                                              \
    {                                                               \
        .uart = 2,        /* RS485 */                               \
        .baud = UART_2_SPEED,                                       \
        .databits = UART_2_DATABITS,                                \
        .parity = UART_2_PARITY,                                    \
        .stop = UART_2_STOP,                                        \
        .tx_pin = 14,                                               \
        .rx_pin = 15,                                               \
        .enabled = true,                                            \
        .pio = 0,                                                   \
    },                                                              \
    {                                                               \
        .uart = 3,        /* RS232 */                               \
        .baud = UART_3_SPEED,                                       \
        .databits = UART_3_DATABITS,                                \
        .parity = UART_3_PARITY,                                    \
        .stop = UART_3_STOP,                                        \
        .tx_pin = 4,                                                \
        .rx_pin = 5,                                                \
        .enabled = true,                                            \
        .pio = -1,                                                  \
    },                                                              \
}

#define OSM_UART_CHANNELS_COUNT 4

#define CMD_UART            0
#define COMMS_UART          1
#define RS485_UART          2
#define RS232_UART          3

#define OSM_UART_BUFFERS_INIT            \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];  \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];\
char uart_1_in_buf[UART_1_IN_BUF_SIZE];  \
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];\
char uart_2_in_buf[UART_2_IN_BUF_SIZE];  \
char uart_2_out_buf[UART_2_OUT_BUF_SIZE];\
char uart_3_in_buf[UART_3_IN_BUF_SIZE];  \
char uart_3_out_buf[UART_3_OUT_BUF_SIZE];

#define OSM_UART_IN_RINGS                               \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),\
    RING_BUF_INIT(uart_2_in_buf, sizeof(uart_2_in_buf)),\
    RING_BUF_INIT(uart_3_in_buf, sizeof(uart_3_in_buf)),\
}

#define OSM_UART_OUT_RINGS                                \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),\
    RING_BUF_INIT(uart_2_out_buf, sizeof(uart_2_out_buf)),\
    RING_BUF_INIT(uart_3_out_buf, sizeof(uart_3_out_buf)),\
}

#define I2S_DIN_PIN             17
#define I2S_BCK_PIN             18
#define I2S_LRCL_PIN            19
