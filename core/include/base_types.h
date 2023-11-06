#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "platform_base_types.h"
#include "config.h"

#ifdef _PICOLIBC__
_Static_assert(_PICOLIBC__ >= 1 && _PICOLIBC_MINOR__>= 7, "Picolibc too old.");
#endif

/*subset of usb_cdc_line_coding_bParityType*/
typedef enum
{
    uart_parity_none = 0,
    uart_parity_odd  = 1,
    uart_parity_even = 2,
} osm_uart_parity_t;

/*matches usb_cdc_line_coding_bCharFormat*/
typedef enum
{
    uart_stop_bits_1   = 0,
    uart_stop_bits_1_5 = 1,
    uart_stop_bits_2   = 2,
} osm_uart_stop_bits_t;

static inline char        osm_uart_parity_as_char(osm_uart_parity_t parity)    { return (parity == uart_parity_even)?'E':(parity == uart_parity_odd)?'O':'N'; }
static inline const char* osm_uart_stop_bits_as_str(osm_uart_stop_bits_t stop) { switch(stop) { case uart_stop_bits_1: return "1"; case uart_stop_bits_1_5: return "1.5"; case uart_stop_bits_2: return "2"; default: return "?"; } }

extern bool osm_decompose_uart_str(char             * str,
                               uint32_t         * speed,
                               uint8_t          * databits,
                               osm_uart_parity_t    * parity,
                               osm_uart_stop_bits_t * stop);

extern char * skip_space(char * pos);
extern char * skip_to_space(char * pos);


/** FLAGS */
/* Flag Masks */
#define IO_PULL_MASK        0x0003
#define IO_STATE_MASK       0x0F00

/* Flags */
#ifndef STM32L4
#define GPIO_PUPD_NONE     0
#define GPIO_PUPD_PULLUP   1
#define GPIO_PUPD_PULLDOWN 2
#else
#include <libopencm3/stm32/gpio.h>
#endif //STM32L4

#define IO_AS_INPUT         0x0100
#define IO_DIR_LOCKED       0x0200
#define IO_OUT_ON           0x0400

/** ENUMS */
/* Enum Masks */
#define IO_ACTIVE_SPECIAL_MASK      0xF000

/* Enums */
typedef enum
{
    IO_SPECIAL_START                    = 0x1000,
    IO_SPECIAL_NONE                     = 0x0000,
    IO_SPECIAL_PULSECOUNT_FALLING_EDGE  = 0x1000,
    IO_SPECIAL_PULSECOUNT_RISING_EDGE   = 0x2000,
    IO_SPECIAL_PULSECOUNT_BOTH_EDGE     = 0x3000,
    IO_SPECIAL_ONEWIRE                  = 0x4000,
    IO_SPECIAL_WATCH                    = 0x5000,
    IO_SPECIAL_MAX                      = IO_SPECIAL_WATCH,
} io_special_t;


extern char* io_get_pull_str(uint16_t io_state);
extern bool  io_is_special(uint16_t io_state);


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
    LIGHT         = 10,
    SOUND         = 11,
    FW_VERSION    = 12,
    FTMA          = 13,
    CUSTOM_0      = 14,
    CUSTOM_1      = 15,
    IO_READING    = 16,
    CONFIG_REVISION = 17,
} measurements_def_type_t;


#define MEASUREMENTS_DEF_NAME_MODBUS            "MODBUS"
#define MEASUREMENTS_DEF_NAME_PM10              "PM10"
#define MEASUREMENTS_DEF_NAME_PM25              "PM25"
#define MEASUREMENTS_DEF_NAME_CURRENT_CLAMP     "CURRENT_CLAMP"
#define MEASUREMENTS_DEF_NAME_W1_PROBE          "W1_PROBE"
#define MEASUREMENTS_DEF_NAME_HTU21D_HUM        "HTU21D_HUM"
#define MEASUREMENTS_DEF_NAME_HTU21D_TMP        "HTU21D_TMP"
#define MEASUREMENTS_DEF_NAME_BAT_MON           "BAT_MON"
#define MEASUREMENTS_DEF_NAME_PULSE_COUNT       "PULSE_COUNT"
#define MEASUREMENTS_DEF_NAME_LIGHT             "LIGHT"
#define MEASUREMENTS_DEF_NAME_SOUND             "SOUND"
#define MEASUREMENTS_DEF_NAME_FW_VERSION        "FW_VERSION"
#define MEASUREMENTS_DEF_NAME_CONFIG_REVISION   "CONFIG_REVISION"
#define MEASUREMENTS_DEF_NAME_FTMA              "FTMA"

#ifndef MEASUREMENTS_DEF_NAME_CUSTOM_0
#define MEASUREMENTS_DEF_NAME_CUSTOM_0          "CUSTOM_0"
#endif // MEASUREMENTS_DEF_NAME_CUSTOM_0

#ifndef MEASUREMENTS_DEF_NAME_CUSTOM_1
#define MEASUREMENTS_DEF_NAME_CUSTOM_1          "CUSTOM_1"
#endif // MEASUREMENTS_DEF_NAME_CUSTOM_1

#define MEASUREMENTS_DEF_NAME_IO_READING        "IO_READING"


typedef struct
{
    char     name[MEASURE_NAME_NULLED_LEN];             // Name of the measurement
    uint8_t  interval;                                  // System intervals happen every 5 mins, this is how many must pass for measurement to be sent
    uint8_t  samplecount;                               // Number of samples in the interval set. Must be greater than or equal to 1
    uint8_t  type:7;                                    // measurement_def_type_t
    uint8_t  is_immediate:1;                            // Should collect as soon to sending as possible.
} measurements_def_t;

#define MODBUS_BLOCK_SIZE 16
#define MODBUS_BLOCKS  ((MODBUS_MEMORY_SIZE / MODBUS_BLOCK_SIZE) - 1) /* We have 1k and the first block is the bus description.*/

#define MODBUS_READ_HOLDING_FUNC 3
#define MODBUS_READ_INPUT_FUNC 4
#define MODBUS_WRITE_SINGLE_HOLDING_FUNC 6
#define MODBUS_WRITE_MULTIPLE_HOLDING_FUNC 16

typedef enum
{
    MODBUS_REG_TYPE_INVALID  = 0,
    MODBUS_REG_TYPE_U16      = 1,
    MODBUS_REG_TYPE_I16      = 2,
    MODBUS_REG_TYPE_U32      = 3,
    MODBUS_REG_TYPE_I32      = 4,
    MODBUS_REG_TYPE_FLOAT    = 5,
    MODBUS_REG_TYPE_MAX = MODBUS_REG_TYPE_FLOAT,
} modbus_reg_type_t;


typedef enum
{
    MODBUS_BYTE_ORDER_MSB = 0,
    MODBUS_BYTE_ORDER_LSB = 1,
} modbus_byte_orders_t;


typedef enum
{
    MODBUS_WORD_ORDER_MSW = 0,
    MODBUS_WORD_ORDER_LSW = 1,
} modbus_word_orders_t;


typedef enum
{
    MB_REG_INVALID = 0,
    MB_REG_WAITING = 1,
    MB_REG_READY   = 2
} modbus_reg_state_t;


typedef struct
{
    char              name[MODBUS_NAME_LEN];
    uint32_t          value_data;
    uint8_t           type; /*modbus_reg_type_t*/
    uint8_t           func:4;
    uint8_t           value_state:4; /*modbus_reg_state_t*/
    uint16_t          reg_addr;
    uint16_t          unit_id;
    uint16_t          next_reg_offset;
} __attribute__((__packed__)) modbus_reg_t;


typedef struct
{
    char           name[MODBUS_NAME_LEN];
    uint8_t        byte_order; /* modbus_byte_orders_t */
    uint8_t        word_order; /* modbus_word_orders_t */
    uint8_t        reg_count;
    uint8_t        _;
    uint16_t       unit_id;
    uint16_t       first_reg_offset;
    uint16_t       next_dev_offset;
    uint16_t       __;
} __attribute__((__packed__)) modbus_dev_t;

typedef struct
{
    uint64_t _;
    uint32_t __;
    uint32_t next_free_offset;
} __attribute__((__packed__)) modbus_free_t;

typedef struct
{
    uint8_t  version;
    uint8_t  binary_protocol; /* BIN or RTU */ /* This is to support pymodbus.framer.binary_framer and http://jamod.sourceforge.net/. */
    uint8_t  databits:4;        /* 8? */
    uint8_t  stopbits:2;        /* osm_uart_stop_bits_t */
    uint8_t  parity:2;          /* osm_uart_parity_t */
    uint8_t  dev_count;
    uint32_t baudrate;
    uint16_t first_dev_offset;
    uint16_t first_free_offset;
    uint32_t _;
    modbus_free_t  blocks[MODBUS_BLOCKS];
} __attribute__((__packed__)) modbus_bus_t;

_Static_assert(((sizeof(modbus_bus_t) % MODBUS_BLOCK_SIZE) == 0) &&
               (sizeof(modbus_free_t) == MODBUS_BLOCK_SIZE) &&
               (sizeof(modbus_dev_t) == MODBUS_BLOCK_SIZE) &&
               (sizeof(modbus_reg_t) == MODBUS_BLOCK_SIZE),
               "Modbus blocks broken.");

typedef enum
{
    ADCS_TYPE_BAT,
    ADCS_TYPE_CC_CLAMP1,
    ADCS_TYPE_CC_CLAMP2,
    ADCS_TYPE_CC_CLAMP3,
    ADCS_TYPE_FTMA1,
    ADCS_TYPE_FTMA2,
    ADCS_TYPE_FTMA3,
    ADCS_TYPE_FTMA4,
    ADCS_TYPE_INVALID,
} adcs_type_t;


typedef enum
{
    COMMS_TYPE_LW = 1,
    COMMS_TYPE_WIFI = 2,
} comms_type_t;


typedef struct
{
    uint8_t type;           /* comms_type_t */
    uint8_t _[127];
} __attribute__((__packed__)) comms_config_t;


typedef enum
{
    MEASUREMENTS_SENSOR_STATE_SUCCESS,
    MEASUREMENTS_SENSOR_STATE_BUSY,
    MEASUREMENTS_SENSOR_STATE_ERROR,
} measurements_sensor_state_t;


typedef union
{
    int64_t v_i64;
    int32_t v_f32;
    char*   v_str;
} measurements_reading_t;


typedef enum
{
    COMMAND_RESP_NONE   = 0x00,
    COMMAND_RESP_OK     = 0x01,
    COMMAND_RESP_ERR    = 0x02,
} command_response_t;


struct cmd_link_t
{
    const char * key;
    const char * desc;
    command_response_t (*cb)(char * args);
    bool hidden;
    struct cmd_link_t * next;
};
