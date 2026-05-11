#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "platform_base_types.h"
#include <osm/core/config.h>

#ifdef _PICOLIBC__
_Static_assert(_PICOLIBC__ >= 1 && _PICOLIBC_MINOR__>= 7, "Picolibc too old.");
#endif

/*subset of usb_cdc_line_coding_bParityType*/
typedef enum
{
    osm_uart_parity_none = 0,
    osm_uart_parity_odd  = 1,
    osm_uart_parity_even = 2,
} osm_osm_uart_parity_t;

/*matches usb_cdc_line_coding_bCharFormat*/
typedef enum
{
    osm_uart_stop_bits_1   = 0,
    osm_uart_stop_bits_1_5 = 1,
    osm_uart_stop_bits_2   = 2,
} osm_osm_uart_stop_bits_t;

static inline char        osm_uart_parity_as_char(osm_osm_uart_parity_t parity)    { return (parity == osm_uart_parity_even)?'E':(parity == osm_uart_parity_odd)?'O':'N'; }
static inline const char* osm_uart_stop_bits_as_str(osm_osm_uart_stop_bits_t stop) { switch(stop) { case osm_uart_stop_bits_1: return "1"; case osm_uart_stop_bits_1_5: return "1.5"; case osm_uart_stop_bits_2: return "2"; default: return "?"; } }

extern bool osm_decompose_uart_str(char             * str,
                               uint32_t         * speed,
                               uint8_t          * databits,
                               osm_osm_uart_parity_t    * parity,
                               osm_osm_uart_stop_bits_t * stop);

char * osm_skip_space(char * pos);
char * osm_skip_to_space(char * pos);


/** FLAGS */
/* Flag Masks */
#define OSM_IO_PULL_MASK        0x0003
#define OSM_IO_STATE_MASK       0x0F00

/* Flags */
#ifndef STM32L4
#define GPIO_PUPD_NONE     0
#define GPIO_PUPD_PULLUP   1
#define GPIO_PUPD_PULLDOWN 2
#else
#include <libopencm3/stm32/gpio.h>
#endif //STM32L4

#define OSM_IO_AS_INPUT         0x0100
#define OSM_IO_DIR_LOCKED       0x0200
#define OSM_IO_OUT_ON           0x0400

/** ENUMS */
/* Enum Masks */
#define OSM_IO_ACTIVE_SPECIAL_MASK      0xF000

/* Enums */
typedef enum
{
    OSM_IO_SPECIAL_START                    = 0x1000,
    OSM_IO_SPECIAL_NONE                     = 0x0000,
    OSM_IO_SPECIAL_PULSECOUNT_FALLING_EDGE  = 0x1000,
    OSM_IO_SPECIAL_PULSECOUNT_RISING_EDGE   = 0x2000,
    OSM_IO_SPECIAL_PULSECOUNT_BOTH_EDGE     = 0x3000,
    OSM_IO_SPECIAL_ONEWIRE                  = 0x4000,
    OSM_IO_SPECIAL_WATCH                    = 0x5000,
    OSM_IO_SPECIAL_MAX                      = OSM_IO_SPECIAL_WATCH,
} osm_io_special_t;


const char* osm_io_get_pull_str(uint16_t io_state);
bool osm_io_is_special(uint16_t io_state);


typedef enum
{
    OSM_MODBUS        = 1,
    OSM_PM10          = 2,
    OSM_PM25          = 3,
    OSM_CURRENT_CLAMP = 4,
    OSM_W1_PROBE      = 5,
    OSM_HTU21D_HUM    = 6,
    OSM_HTU21D_TMP    = 7,
    OSM_BAT_MON       = 8,
    OSM_PULSE_COUNT   = 9,
    OSM_LIGHT         = 10,
    OSM_SOUND         = 11,
    OSM_FW_VERSION    = 12,
    OSM_FTMA          = 13,
    OSM_CUSTOM_0      = 14,
    OSM_CUSTOM_1      = 15,
    OSM_IO_READING    = 16,
    OSM_CONFIG_REVISION = 17,
    OSM_SENxx = 18,
    OSM_EXAMPLE_RS232         = 19,
    OSM_TMP4718       = 20,
} osm_measurements_def_type_t;


#define OSM_MEASUREMENTS_DEF_NAME_MODBUS            "MODBUS"
#define OSM_MEASUREMENTS_DEF_NAME_PM10              "PM10"
#define OSM_MEASUREMENTS_DEF_NAME_PM25              "PM25"
#define OSM_MEASUREMENTS_DEF_NAME_CURRENT_CLAMP     "CURRENT_CLAMP"
#define OSM_MEASUREMENTS_DEF_NAME_W1_PROBE          "W1_PROBE"
#define OSM_MEASUREMENTS_DEF_NAME_HTU21D_HUM        "HTU21D_HUM"
#define OSM_MEASUREMENTS_DEF_NAME_HTU21D_TMP        "HTU21D_TMP"
#define OSM_MEASUREMENTS_DEF_NAME_BAT_MON           "BAT_MON"
#define OSM_MEASUREMENTS_DEF_NAME_PULSE_COUNT       "PULSE_COUNT"
#define OSM_MEASUREMENTS_DEF_NAME_LIGHT             "LIGHT"
#define OSM_MEASUREMENTS_DEF_NAME_SOUND             "SOUND"
#define OSM_MEASUREMENTS_DEF_NAME_FW_VERSION        "FW_VERSION"
#define OSM_MEASUREMENTS_DEF_NAME_CONFIG_REVISION   "CONFIG_REVISION"
#define OSM_MEASUREMENTS_DEF_NAME_FTMA              "FTMA"

#ifndef OSM_MEASUREMENTS_DEF_NAME_CUSTOM_0
#define OSM_MEASUREMENTS_DEF_NAME_CUSTOM_0          "CUSTOM_0"
#endif // OSM_MEASUREMENTS_DEF_NAME_CUSTOM_0

#ifndef OSM_MEASUREMENTS_DEF_NAME_CUSTOM_1
#define OSM_MEASUREMENTS_DEF_NAME_CUSTOM_1          "CUSTOM_1"
#endif // OSM_MEASUREMENTS_DEF_NAME_CUSTOM_1

#define OSM_MEASUREMENTS_DEF_NAME_IO_READING        "IO_READING"


typedef struct
{
    char     name[OSM_MEASURE_NAME_NULLED_LEN];             // Name of the measurement
    uint8_t  interval;                                  // System intervals happen every 5 mins, this is how many must pass for measurement to be sent
    uint8_t  samplecount;                               // Number of samples in the interval set. Must be greater than or equal to 1
    uint8_t  type:7;                                    // measurement_def_type_t
    uint8_t  is_immediate:1;                            // Should collect as soon to sending as possible.
} osm_measurements_def_t;

#define OSM_MODBUS_BLOCK_SIZE 16
#define OSM_MODBUS_BLOCKS  ((OSM_MODBUS_MEMORY_SIZE / OSM_MODBUS_BLOCK_SIZE) - 1) /* We have 1k and the first block is the bus description.*/

#define OSM_MODBUS_READ_HOLDING_FUNC 3
#define OSM_MODBUS_READ_INPUT_FUNC 4
#define OSM_MODBUS_WRITE_SINGLE_HOLDING_FUNC 6
#define OSM_MODBUS_WRITE_MULTIPLE_HOLDING_FUNC 16

typedef enum
{
    OSM_MODBUS_REG_TYPE_INVALID  = 0,
    OSM_MODBUS_REG_TYPE_U16      = 1,
    OSM_MODBUS_REG_TYPE_I16      = 2,
    OSM_MODBUS_REG_TYPE_U32      = 3,
    OSM_MODBUS_REG_TYPE_I32      = 4,
    OSM_MODBUS_REG_TYPE_FLOAT    = 5,
    OSM_MODBUS_REG_TYPE_MAX = OSM_MODBUS_REG_TYPE_FLOAT,
} osm_modbus_reg_type_t;


typedef enum
{
    OSM_MODBUS_BYTE_ORDER_MSB = 0,
    OSM_MODBUS_BYTE_ORDER_LSB = 1,
} osm_modbus_byte_orders_t;


typedef enum
{
    OSM_MODBUS_WORD_ORDER_MSW = 0,
    OSM_MODBUS_WORD_ORDER_LSW = 1,
} osm_modbus_word_orders_t;


typedef enum
{
    OSM_MB_REG_INVALID = 0,
    OSM_MB_REG_WAITING = 1,
    OSM_MB_REG_READY   = 2
} osm_modbus_reg_state_t;


typedef struct
{
    char              name[OSM_MODBUS_NAME_LEN];
    uint32_t          value_data;
    uint8_t           type; /*osm_modbus_reg_type_t*/
    uint8_t           func:4;
    uint8_t           value_state:4; /*osm_modbus_reg_state_t*/
    uint16_t          reg_addr;
    uint16_t          unit_id;
    uint16_t          next_reg_offset;
} __attribute__((__packed__)) osm_modbus_reg_t;


typedef struct
{
    char           name[OSM_MODBUS_NAME_LEN];
    uint8_t        byte_order; /* osm_modbus_byte_orders_t */
    uint8_t        word_order; /* osm_modbus_word_orders_t */
    uint8_t        reg_count;
    uint8_t        _;
    uint16_t       unit_id;
    uint16_t       first_reg_offset;
    uint16_t       next_dev_offset;
    uint16_t       __;
} __attribute__((__packed__)) osm_modbus_dev_t;

typedef struct
{
    uint64_t _;
    uint32_t __;
    uint32_t next_free_offset;
} __attribute__((__packed__)) osm_modbus_free_t;

typedef struct
{
    uint8_t  version;
    uint8_t  binary_protocol; /* BIN or RTU */ /* This is to support pymodbus.framer.binary_framer and http://jamod.sourceforge.net/. */
    uint8_t  databits:4;        /* 8? */
    uint8_t  stopbits:2;        /* osm_osm_uart_stop_bits_t */
    uint8_t  parity:2;          /* osm_osm_uart_parity_t */
    uint8_t  dev_count;
    uint32_t baudrate;
    uint16_t first_dev_offset;
    uint16_t first_free_offset;
    uint32_t _;
    osm_modbus_free_t  blocks[OSM_MODBUS_BLOCKS];
} __attribute__((__packed__)) osm_modbus_bus_t;

_Static_assert(((sizeof(osm_modbus_bus_t) % OSM_MODBUS_BLOCK_SIZE) == 0) &&
               (sizeof(osm_modbus_free_t) == OSM_MODBUS_BLOCK_SIZE) &&
               (sizeof(osm_modbus_dev_t) == OSM_MODBUS_BLOCK_SIZE) &&
               (sizeof(osm_modbus_reg_t) == OSM_MODBUS_BLOCK_SIZE),
               "Modbus blocks broken.");

typedef enum
{
    OSM_ADCS_TYPE_BAT,
    OSM_ADCS_TYPE_CC_CLAMP1,
    OSM_ADCS_TYPE_CC_CLAMP2,
    OSM_ADCS_TYPE_CC_CLAMP3,
    OSM_ADCS_TYPE_FTMA1,
    OSM_ADCS_TYPE_FTMA2,
    OSM_ADCS_TYPE_FTMA3,
    OSM_ADCS_TYPE_FTMA4,
    OSM_ADCS_TYPE_INVALID,
} osm_adcs_type_t;


typedef enum
{
    OSM_COMMS_TYPE_UNKNOWN = 0,
    OSM_COMMS_TYPE_LW = 1,
    OSM_COMMS_TYPE_WIFI = 2,
    OSM_COMMS_TYPE_POE = 3,
    OSM_COMMS_TYPE_COUNT,
} osm_comms_type_t;


typedef struct
{
    uint8_t type;           /* osm_comms_type_t */
    uint8_t _[511];
} __attribute__((__packed__)) osm_comms_config_t;


typedef enum
{
    OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS,
    OSM_MEASUREMENTS_SENSOR_STATE_BUSY,
    OSM_MEASUREMENTS_SENSOR_STATE_ERROR,
} osm_measurements_sensor_state_t;


typedef union
{
    int64_t v_i64;
    int32_t v_f32;
    char*   v_str;
} osm_measurements_reading_t;


typedef enum
{
    OSM_COMMAND_RESP_OK     = 0x00,
    OSM_COMMAND_RESP_ERR    = 0x01,
} osm_command_response_t;


typedef struct osm_cmd_ctx_t osm_cmd_ctx_t;
struct osm_cmd_ctx_t
{
    void (*output_cb)(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
    void (*error_cb)(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
    void (*flush_cb)(osm_cmd_ctx_t * ctx);
};


struct osm_cmd_link_t
{
    const char * key;
    const char * desc;
    osm_command_response_t (*cb)(char * args, osm_cmd_ctx_t * ctx);
    bool hidden;
    struct osm_cmd_link_t * next;
};


typedef enum
{
    OSM_IO_PUPD_NONE    = 0,
    OSM_IO_PUPD_UP      = 1,
    OSM_IO_PUPD_DOWN    = 2,
} osm_io_pupd_t;
