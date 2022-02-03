#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/*subset of usb_cdc_line_coding_bParityType*/
typedef enum
{
    uart_parity_none = 0,
    uart_parity_odd  = 1,
    uart_parity_even = 2,
} uart_parity_t;

/*matches usb_cdc_line_coding_bCharFormat*/
typedef enum
{
    uart_stop_bits_1   = 0,
    uart_stop_bits_1_5 = 1,
    uart_stop_bits_2   = 2,
} uart_stop_bits_t;

static inline char        uart_parity_as_char(uart_parity_t parity)    { return (parity == uart_parity_even)?'E':(parity == uart_parity_odd)?'O':'N'; }
static inline const char* uart_stop_bits_as_str(uart_stop_bits_t stop) { switch(stop) { case uart_stop_bits_1: return "1"; case uart_stop_bits_1_5: return "1.5"; case uart_stop_bits_2: return "2"; default: return "?"; } }

extern bool decompose_uart_str(char             * str,
                               uint32_t         * speed,
                               uint8_t          * databits,
                               uart_parity_t    * parity,
                               uart_stop_bits_t * stop);

extern char * skip_space(char * pos);

#ifdef STM32L4
#include <libopencm3/stm32/rcc.h>

typedef struct
{
    uint32_t port;
    uint32_t pins;
} port_n_pins_t;

#define PORT_TO_RCC(_port_)   (RCC_GPIOA + ((_port_ - GPIO_PORT_A_BASE) / 0x400))


typedef struct
{
    uint32_t              usart;
    enum rcc_periph_clken uart_clk;
    uint32_t              baud;
    uint8_t               databits:4;
    uint8_t               parity:2 /*uart_parity_t*/;
    uint8_t               stop:2 /*uart_stop_bits_t*/;
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
} uart_channel_t;

typedef struct
{
    uint32_t rcc;
    uint32_t i2c;
    uint32_t speed;
    uint32_t clock_megahz;
    uint32_t gpio_func;
    port_n_pins_t port_n_pins;
} i2c_def_t;

#endif //STM32L4


typedef enum
{
    MODBUS        = 1,
    PM10          = 2,
    PM25          = 3,
    CURRENT_CLAMP = 4,
    W1_PROBE      = 5,
    HTU21D_HUM    = 6,
    HTU21D_TMP    = 7,
    BAT_MON       = 8,
    PULSE_COUNT   = 9,
} measurement_def_type_t;


typedef struct
{
    char     name[MEASURE_NAME_LEN + 1];                // Name of the measurement
    uint8_t  interval;                                  // System intervals happen every 5 mins, this is how many must pass for measurement to be sent
    uint8_t  samplecount;                               // Number of samples in the interval set. Must be greater than or equal to 1
    uint8_t  type;                                      // measurement_def_type_t
} measurement_def_t;

#define MODBUS_READ_HOLDING_FUNC 3
#define MODBUS_READ_INPUT_FUNC 4

typedef enum
{
    MODBUS_REG_TYPE_INVALID  = 0,
    MODBUS_REG_TYPE_U16      = 1,
    MODBUS_REG_TYPE_U32      = 2,
    MODBUS_REG_TYPE_FLOAT    = 3,
    MODBUS_REG_TYPE_MAX = MODBUS_REG_TYPE_FLOAT,
} modbus_reg_type_t;


typedef struct
{
    char              name[MODBUS_NAME_LEN];
    uint32_t          class_data_a; /* what ever child class wants */
    uint8_t           type; /*modbus_reg_type_t*/
    uint8_t           func;
    uint16_t          reg_addr;
    uint32_t          class_data_b; /* what ever child class wants */
} __attribute__((__packed__)) modbus_reg_t;


typedef struct
{
    char           name[MODBUS_NAME_LEN];
    uint8_t        slave_id;
    uint8_t        _;
    uint16_t       __; /* pad.*/
    modbus_reg_t   regs[MODBUS_DEV_REGS];
} __attribute__((__packed__)) modbus_dev_t;


typedef struct
{
    uint8_t version;
    uint8_t max_dev_num;
    uint8_t max_reg_num;
    uint8_t _;
    uint32_t __;
    uint32_t baudrate;
    uint8_t  binary_protocol; /* BIN or RTU */
    uint8_t  databits;        /* 8? */
    uint8_t  stopbits;        /* uart_stop_bits_t */
    uint8_t  parity;          /* uart_parity_t */
    modbus_dev_t            modbus_devices[MODBUS_MAX_DEV];
} __attribute__((__packed__)) modbus_bus_t;
