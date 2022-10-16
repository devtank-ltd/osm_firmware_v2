#pragma once

#define ADC_INDEX_CURRENT_CLAMP_1 0
#define ADC_INDEX_CURRENT_CLAMP_2 1
#define ADC_INDEX_CURRENT_CLAMP_3 2
#define ADC_INDEX_BAT_MON         3
#define ADC_INDEX_3V3_RAIL_MON    4
#define ADC_INDEX_5V_RAIL_MON     5

#define ADC_COUNT       6
#define ADC_CC_COUNT    3

#define CMD_UART   0
#define LW_UART    1
#define HPM_UART   2
#define RS485_UART 3

#define UART_CHANNELS_COUNT 4

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
