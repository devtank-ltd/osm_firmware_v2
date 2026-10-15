#pragma once


#define IOS_PORT_N_PINS     \
{                           \
    { 2  },                 \
    { 5  },                 \
    { 6  },                 \
    { 7  },                 \
    { 8  },                 \
    { 10 },                 \
    { 11 },                 \
    { 24 },                 \
    { 26 },                 \
    { 27 },                 \
    { 30 },                 \
    { 31 },                 \
    { 32 },                 \
}

#define IOS_STATE                                                      \
{                                                                      \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
    OSM_IO_AS_INPUT,                                                   \
}


#define IOS_NAMES                                                  \
{                                                                  \
    "IO01_RP02",                                                   \
    "IO02_RP05",                                                   \
    "IO03_RP06",                                                   \
    "IO04_RP07",                                                   \
    "IO05_RP08",                                                   \
    "IO06_RP10",                                                   \
    "IO07_RP11",                                                   \
    "IO08_RP24",                                                   \
    "IO09_RP26",                                                   \
    "IO10_RP27",                                                   \
    "IO11_RP30",                                                   \
    "IO12_RP31",                                                   \
    "IO13_RP32",                                                   \
}

typedef struct
{
    const char *name;
    const char *unit;
    osm_adc_coeffs_t coeffs;
} adc_init_t;

#define ADCS_INIT                                   \
{                                                   \
    {"ADC0", "mV", {1.241f, 0.2f, 0.f, 0.f}},       \
    {"ADC1", "mV", {1.241f, 0.2f, 0.f, 0.f}},       \
    {"ADC2", "mV", {1.241f, 0.2f, 0.f, 0.f}},       \
    {"ADC3", "mV", {1.241f, 0.2f, 0.f, 0.f}},       \
}


#define COMMS_RESET_PORT_N

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
        .enabled = true,                                            \
        .en_usb = true,                                             \
        .index = 0,                                                 \
    },                                                              \
}

#define OSM_UART_CHANNELS_COUNT 2

#define OSM_UART_BUFFERS_INIT             \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];   \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE]; \
char uart_1_in_buf[UART_1_IN_BUF_SIZE];   \
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];

#define OSM_UART_IN_RINGS                               \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),\
}

#define OSM_UART_OUT_RINGS                                \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),\
}
