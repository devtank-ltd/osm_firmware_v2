#pragma once


#define CMD_UART   0
#define COMMS_UART 1
#define HPM_UART   2
#define EXT_UART   3

#define UART_CHANNELS_COUNT 4

#define ADC_COUNT 10

#define ADC_INDEX_BAT_MON         0
#define ADC_INDEX_CURRENT_CLAMP_1 1
#define ADC_INDEX_CURRENT_CLAMP_2 2
#define ADC_INDEX_CURRENT_CLAMP_3 3
#define ADC_INDEX_FTMA_1          4
#define ADC_INDEX_FTMA_2          5
#define ADC_INDEX_FTMA_3          6
#define ADC_INDEX_FTMA_4          7


#define ADC_TYPES_ALL_CC { ADCS_TYPE_CC_CLAMP1,  \
                           ADCS_TYPE_CC_CLAMP2,  \
                           ADCS_TYPE_CC_CLAMP3   }


#define ADC_TYPES_ALL_FTMA { ADCS_TYPE_FTMA1,    \
                             ADCS_TYPE_FTMA2,    \
                             ADCS_TYPE_FTMA3,    \
                             ADCS_TYPE_FTMA4     }


#define ADC_FTMA_CHANNELS { ADC1_CHANNEL_FTMA_1,  \
                            ADC1_CHANNEL_FTMA_2,  \
                            ADC1_CHANNEL_FTMA_3,  \
                            ADC1_CHANNEL_FTMA_4   }


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

#define IOS_PORT_N_PINS            \
{                                  \
    {0},   /* IO 0  */ \
    {1},   /* IO 1 */ \
    {2},   /* IO 2  */ \
    {3},   /* IO 3  */ \
    {4},   /* IO 4  */ \
    {5},   /* IO 5  */ \
    {6},   /* IO 6  */ \
    {7},   /* IO 7  */ \
    {8},   /* IO 8  */ \
    {9},   /* IO 9 */ \
}

#define IOS_STATE                                                      \
{                                                                      \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 0   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 1   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 2   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 3   */ \
    IO_AS_INPUT,                                        /* GPIO 4   */ \
    IO_AS_INPUT,                                        /* GPIO 5   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 6   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 7   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 8   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 9   */ \
}

#define ADC_TYPES_ALL_CC { ADCS_TYPE_CC_CLAMP1,  \
                           ADCS_TYPE_CC_CLAMP2,  \
                           ADCS_TYPE_CC_CLAMP3   }

#define ADC_TYPES_ALL_FTMA { ADCS_TYPE_FTMA1,    \
                             ADCS_TYPE_FTMA2,    \
                             ADCS_TYPE_FTMA3,    \
                             ADCS_TYPE_FTMA4     }

#define W1_PULSE_1_IO               4
#define W1_PULSE_2_IO               5

#define IOS_WATCH_COUNT 2
#define IOS_WATCH_IOS                       { W1_PULSE_1_IO, W1_PULSE_2_IO }

#define HTU21D_I2C                  1
#define HTU21D_I2C_INDEX            0
#define VEML7700_I2C                1
#define VEML7700_I2C_INDEX          0

#define DS18B20_INSTANCES   {                                          \
    { { MEASUREMENTS_W1_PROBE_NAME_1, W1_PULSE_1_IO} ,                 \
        0 },                                                           \
}

#define     post_init()


#define UART_1_SPEED 9600
#define UART_2_SPEED 115200
#define UART_3_SPEED 9600

