#pragma once

#define ADC_INDEX_CURRENT_CLAMP_1 0
#define ADC_INDEX_CURRENT_CLAMP_2 1
#define ADC_INDEX_CURRENT_CLAMP_3 2
#define ADC_INDEX_BAT_MON         3
#define ADC_INDEX_3V3_RAIL_MON    4
#define ADC_INDEX_5V_RAIL_MON     5
#define ADC_INDEX_FTMA1           6
#define ADC_INDEX_FTMA2           7
#define ADC_INDEX_FTMA3           8
#define ADC_INDEX_FTMA4           9

#define ADC_COUNT       6
#define ADC_CC_COUNT    3

#define ADC_FTMA_COUNT  4

#define CMD_UART   0
#define COMMS_UART 1
#define HPM_UART   2
#define RS485_UART 3

#define UART_CHANNELS_COUNT 4

#define UART_BUFFERS_INIT                \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];  \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];\
char uart_1_in_buf[UART_1_IN_BUF_SIZE];  \
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];\
char uart_2_in_buf[UART_2_IN_BUF_SIZE];  \
char uart_2_out_buf[UART_2_OUT_BUF_SIZE];\
char uart_3_in_buf[UART_3_IN_BUF_SIZE];  \
char uart_3_out_buf[UART_3_OUT_BUF_SIZE];

#define UART_IN_RINGS                                   \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),\
    RING_BUF_INIT(uart_2_in_buf, sizeof(uart_2_in_buf)),\
    RING_BUF_INIT(uart_3_in_buf, sizeof(uart_3_in_buf)),\
}

#define UART_OUT_RINGS                                    \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),\
    RING_BUF_INIT(uart_2_out_buf, sizeof(uart_2_out_buf)),\
    RING_BUF_INIT(uart_3_out_buf, sizeof(uart_3_out_buf)),}



#define IOS_COUNT 10
#define W1_PULSE_1_IO               4
#define W1_PULSE_2_IO               5


#define IOS_PORT_N_PINS    \
{                          \
    {0},   /* IO 0  */ \
    {1},   /* IO 1 */  \
    {2},   /* IO 2  */ \
    {3},   /* IO 3  */ \
    {4},   /* IO 4  */ \
    {5},   /* IO 5  */ \
    {6},   /* IO 6  */ \
    {7},   /* IO 7  */ \
    {8},   /* IO 8  */ \
    {9},   /* IO 9 */  \
}


#define IOS_STATE                                                      \
{                                                                      \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 0   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 1   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 2   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 3   */ \
    IO_AS_INPUT | IO_TYPE_PULSECOUNT | IO_TYPE_ONEWIRE, /* GPIO 4   */ \
    IO_AS_INPUT | IO_TYPE_PULSECOUNT | IO_TYPE_ONEWIRE, /* GPIO 5   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 6   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 7   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 8   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 9   */ \
}
