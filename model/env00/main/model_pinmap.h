#pragma once

#define DE_485_PIN    GPIO_NUM_25


#define ENV00_UART_CHANNELS                                         \
{                                                                   \
    {                                                               \
        .enabled = true,                                            \
        .uart = UART_NUM_0,        /* DEBUG */                      \
        .tx_pin = GPIO_NUM_1,                                       \
        .rx_pin = GPIO_NUM_3,                                       \
        .config =                                                   \
        {                                                           \
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,          \
            .data_bits = UART_DATA_8_BITS,                          \
            .parity = UART_PARITY_DISABLE,                          \
            .stop_bits = UART_STOP_BITS_1,                          \
            .source_clk = UART_SCLK_REF_TICK,                       \
        },                                                          \
    },                                                              \
    {                                                               \
        .enabled = true,                                            \
        .uart = UART_NUM_2,        /* Modbus */                     \
        .tx_pin = GPIO_NUM_16,                                      \
        .rx_pin = GPIO_NUM_17,                                      \
        .config =                                                   \
        {                                                           \
            .baud_rate = 9600,                                      \
            .data_bits = UART_DATA_8_BITS,                          \
            .parity = UART_PARITY_DISABLE,                          \
            .stop_bits = UART_STOP_BITS_1,                          \
        },                                                          \
    }                                                               \
}

#define UART_CHANNELS_COUNT 2

#define I2C_BUSES                                                   \
{                                                                   \
    {                                                               \
        I2C_NUM_0,                                                  \
        {                                                           \
            .mode = I2C_MODE_MASTER,                                \
            .sda_io_num = GPIO_NUM_21,                              \
            .scl_io_num = GPIO_NUM_22,                              \
            .sda_pullup_en = GPIO_PULLUP_ENABLE,                    \
            .scl_pullup_en = GPIO_PULLUP_ENABLE,                    \
            .master.clk_speed = 100000,                             \
        }                                                           \
    }                                                               \
}


#define IOS_COUNT 0


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

