/*
Datasheet:
    https://eu.mouser.com/datasheet/2/427/VISH_S_A0012091125_1-2572303.pdf
        (Accessed: 31.01.2022)
Application Guide:
    https://www.vishay.com/docs/84323/designingveml7700.pdf
        (Accessed: 15.02.2022)
*/


#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "veml7700.h"
#include "config.h"
#include "i2c.h"
#include "log.h"
#include "measurements.h"
#include "common.h"
#include "uart_rings.h"
#include "timers.h"
#include "pinmap.h"


#define I2C_VEML7700_ADDR   0x10

#define VEML7700_COLLECTION_TIME_OFFSET_MS      10
#define VEML7700_RES_SCALE                      10000
#define VEML7700_COUNT_LOWER_THRESHOLD          100
#define VEML7700_DEFAULT_COLLECT_TIME           6600  // Max time in ms
#define VEML7700_MAX_READ_TIME                  VEML7700_DEFAULT_COLLECT_TIME + 100
#define VEML7700_TIMEOUT_TIME_MS                1000


typedef enum
{
    VEML7700_CMD_ALS_CONF_0     = 0x00,
    VEML7700_CMD_ALS_WH         = 0x01,
    VEML7700_CMD_ALS_WL         = 0x02,
    VEML7700_CMD_POWER_SAVING   = 0x03,
    VEML7700_CMD_ALS            = 0x04,
    VEML7700_CMD_WHITE          = 0x05,
    VEML7700_CMD_ALS_INT        = 0x06,
} veml7700_cmd_t;


typedef enum
{
    VEML7700_CONF_ALS_SM_GAIN_1   = 0 , /* 0b00 */
    VEML7700_CONF_ALS_SM_GAIN_2   = 1 , /* 0b01 */
    VEML7700_CONF_ALS_SM_GAIN_1_8 = 2 , /* 0b10 */
    VEML7700_CONF_ALS_SM_GAIN_1_4 = 3 , /* 0b11 */
} veml7700_als_gains_t;


typedef enum
{
    VEML7700_CONF_ALS_IT_25  = 0xC , /* 0b1100 */
    VEML7700_CONF_ALS_IT_50  = 0x8 , /* 0b1000 */
    VEML7700_CONF_ALS_IT_100 = 0x0 , /* 0b0000 */
    VEML7700_CONF_ALS_IT_200 = 0x1 , /* 0b0001 */
    VEML7700_CONF_ALS_IT_400 = 0x2 , /* 0b0010 */
    VEML7700_CONF_ALS_IT_800 = 0x3 , /* 0b0011 */
} veml7700_als_int_times_t;


typedef enum
{
    VEML7700_CONF_ALS_PERS_1 = 0, /* 0b00 */
    VEML7700_CONF_ALS_PERS_2 = 1, /* 0b01 */
    VEML7700_CONF_ALS_PERS_4 = 2, /* 0b10 */
    VEML7700_CONF_ALS_PERS_8 = 3, /* 0b11 */
} veml7700_als_persitent_t;


typedef enum
{
    VEML7700_CONF_ALS_INT_EN_DIABLED = 0 ,
    VEML7700_CONF_ALS_INT_EN_ENABLED = 1 ,
} veml7700_als_int_en_t;


typedef enum
{
    VEML7700_CONF_ALS_SD_ON  = 0 ,
    VEML7700_CONF_ALS_SD_OFF = 1 ,
} veml7700_als_shutdown_t;


typedef enum
{
    VEML7700_PWR_PSM_MODE_1 = 0 , /* 0b00 */
    VEML7700_PWR_PSM_MODE_2 = 1 , /* 0b01 */
    VEML7700_PWR_PSM_MODE_3 = 2 , /* 0b10 */
    VEML7700_PWR_PSM_MODE_4 = 3 , /* 0b11 */
} veml7700_pwr_psm_t;


typedef enum
{
    VEML7700_PWR_PSM_EN_DISABLE = 0 ,
    VEML7700_PWR_PSM_EN_ENABLE  = 1 ,
} veml7700_pwr_en_t;


typedef enum
{
    VEML7700_STATE_OFF      ,
    VEML7700_STATE_READING  ,
    VEML7700_STATE_DONE     ,
} veml7700_state_t;


typedef struct
{
    veml7700_als_gains_t        code;
    uint8_t                     resolution_multiplier;
} veml7700_gain_t;


typedef struct
{
    veml7700_als_int_times_t    code;
    uint8_t                     resolution_multiplier;
    uint32_t                    wait_time;
} veml7700_it_ms_t;


typedef union
{
    struct
    {
        uint16_t als_sd:1;
        uint16_t als_int_en:1;
        // cppcheck-suppress unusedStructMember ; Padding
        uint16_t _:2;
        uint16_t als_pers:2;
        uint16_t als_it:4;
        // cppcheck-suppress unusedStructMember ; Padding
        uint16_t __:1;
        uint16_t als_sm:2;
        // cppcheck-suppress unusedStructMember ; Padding
        uint16_t ___:3;
    };
    uint16_t raw;
} __attribute__((__packed__)) veml7700_cmd_conf_t;


typedef union
{
    struct
    {
        uint16_t psm_en:1;
        uint16_t psm:2;
        // cppcheck-suppress unusedStructMember ; Padding
        uint16_t _:13;
    };
    uint16_t raw;
} __attribute__((__packed__)) veml7700_cmd_pwr_t;


typedef struct
{
    veml7700_cmd_conf_t     config;
    veml7700_cmd_pwr_t      power;
    uint8_t                 gain_index;
    uint8_t                 int_index;
    uint16_t                resolution_scaled; // scaled by VEML7700_RES_SCALE
    uint32_t                wait_time;
} veml7700_conf_t;


typedef struct
{
    uint32_t                lux;
    bool                    is_valid;
} veml7700_reading_t;


typedef struct
{
    veml7700_state_t        state;
    uint32_t                last_read;
} veml7700_sensor_state_t;


typedef struct
{
    uint32_t                start_time;
    uint32_t                last_time_taken;
} veml7700_time_t;



static const veml7700_gain_t    _veml7700_gains[]               = { { VEML7700_CONF_ALS_SM_GAIN_1_8, 16} ,
                                                                    { VEML7700_CONF_ALS_SM_GAIN_1_4,  8} ,
                                                                    { VEML7700_CONF_ALS_SM_GAIN_1  ,  2} ,
                                                                    { VEML7700_CONF_ALS_SM_GAIN_2  ,  1} };

static const veml7700_it_ms_t   _veml7700_integration_times[]   = { { VEML7700_CONF_ALS_IT_25 , 32,  25 } ,
                                                                    { VEML7700_CONF_ALS_IT_50 , 16,  50 } ,
                                                                    { VEML7700_CONF_ALS_IT_100,  8, 100 } ,
                                                                    { VEML7700_CONF_ALS_IT_200,  4, 200 } ,
                                                                    { VEML7700_CONF_ALS_IT_400,  2, 400 } ,
                                                                    { VEML7700_CONF_ALS_IT_800,  1, 800 } };

#define VEML7700_GAINS_COUNT (ARRAY_SIZE(_veml7700_gains))
#define VEML7700_INT_COUNT (ARRAY_SIZE(_veml7700_integration_times))

static const veml7700_conf_t    _veml7700_default_ctx   = {.config={.als_sd     = VEML7700_CONF_ALS_SD_OFF,
                                                                    .als_int_en = VEML7700_CONF_ALS_INT_EN_DIABLED,
                                                                    .als_pers   = VEML7700_CONF_ALS_PERS_1,
                                                                    .als_it     = VEML7700_CONF_ALS_IT_100,
                                                                    .als_sm     = VEML7700_CONF_ALS_SM_GAIN_1_8},
                                                           .power={ .psm_en = VEML7700_PWR_PSM_EN_DISABLE,
                                                                    .psm    = VEML7700_PWR_PSM_MODE_1},
                                                           .gain_index = 0,
                                                           .int_index  = 2 };

static veml7700_conf_t          _veml7700_ctx           = _veml7700_default_ctx;

static veml7700_reading_t       _veml7700_reading       = {.lux=0,
                                                           .is_valid=false};

static veml7700_sensor_state_t  _veml7700_state_machine = {.state=VEML7700_STATE_OFF,
                                                           .last_read=0};

static veml7700_time_t          _veml7700_time          = {.start_time=0,
                                                           .last_time_taken=VEML7700_DEFAULT_COLLECT_TIME};


static void _veml7700_get_u16(uint8_t d[2], uint16_t *r)
{
    *r = (d[1] << 8) | d[0];
    light_debug("Got [0x%"PRIx8", 0x%"PRIx8"] = %"PRIu16, d[0], d[1], *r);
}


static bool _veml7700_read_reg16(veml7700_cmd_t reg, uint16_t * r)
{
    if (!r)
    {
        return false;
    }
    uint8_t reg8 = reg;
    uint8_t d[2] = {0};
    light_debug("Read command 0x%"PRIx8, reg8);
    if (!i2c_transfer_timeout(VEML7700_I2C, I2C_VEML7700_ADDR, &reg8, 1, d, 2, 100))
    {
        light_debug("Read timed out.");
        return false;
    }
    _veml7700_get_u16(d, r);
    return true;
}


static bool _veml7700_write_reg16(veml7700_cmd_t reg, uint16_t data)
{
    uint8_t payload[3] = { reg, data & 0xFF, data >> 8 };
    light_debug("Send command 0x%"PRIx8" [0x%"PRIx8" 0x%"PRIx8"].", payload[0], payload[1], payload[2]);
    if (!i2c_transfer_timeout(VEML7700_I2C, I2C_VEML7700_ADDR, payload, 3, NULL, 0, 100))
    {
        light_debug("Write timed out.");
        return false;
    }
    return true;
}


static uint16_t _veml7700_get_resolution(void)
{
    uint16_t resolution_scaled = 0.0036 * VEML7700_RES_SCALE;
    uint8_t multiplier = 1;
    multiplier *= _veml7700_gains[_veml7700_ctx.gain_index].resolution_multiplier;
    multiplier *= _veml7700_integration_times[_veml7700_ctx.int_index].resolution_multiplier;
    return resolution_scaled * multiplier;
}


static uint32_t _veml7700_get_wait_time(void)
{
    uint32_t wait_time = _veml7700_integration_times[_veml7700_ctx.int_index].wait_time;
    wait_time *= 1.1;
    return wait_time;
}


static void _veml7700_reset_ctx(void)
{
    _veml7700_ctx = _veml7700_default_ctx;
}


static bool _veml7700_set_config(void)
{
    veml7700_als_gains_t gain_code = _veml7700_gains[_veml7700_ctx.gain_index].code;
    _veml7700_ctx.config.als_sm = gain_code;
    veml7700_als_int_times_t int_code = _veml7700_integration_times[_veml7700_ctx.int_index].code;
    _veml7700_ctx.config.als_it = int_code;
    if (!_veml7700_write_reg16(VEML7700_CMD_POWER_SAVING, _veml7700_ctx.power.raw ) ||
        !_veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0,   _veml7700_ctx.config.raw) )
    {
        light_debug("Set config failed.");
        return false;
    }
    _veml7700_ctx.resolution_scaled = _veml7700_get_resolution();
    _veml7700_ctx.wait_time = _veml7700_get_wait_time();
    return true;
}


static bool _veml7700_turn_on(void)
{
    _veml7700_ctx.config.als_sd = VEML7700_CONF_ALS_SD_ON;
    return _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, _veml7700_ctx.config.raw);
}


static bool _veml7700_turn_off(void)
{
    _veml7700_ctx.config.als_sd = VEML7700_CONF_ALS_SD_OFF;
    return _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, _veml7700_ctx.config.raw);
}


static uint16_t _veml7700_read_als(void)
{
    uint16_t als_data = 0;
    _veml7700_read_reg16(VEML7700_CMD_ALS, &als_data);
    return als_data;
}


#ifndef VEML7700_DEVTANK_CORRECTED
static bool _veml7700_conv(uint32_t* lux_corrected, uint16_t counts)
{
    /**
     lux = counts * resolution
     lux_corrected = Ax^4 + Bx^3 + Cx^2 + Dx
           where x = lux
     */
    uint64_t lux = (counts * _veml7700_ctx.resolution_scaled) / VEML7700_RES_SCALE;
    light_debug("Lux before correction = %"PRIu64, lux);
    const float A = +6.0135E-13f;
    const float B = -9.3924E-09f;
    const float C = +8.1488E-05f;
    const float D = +1.0023E+00f;
    lux =   (A * lux * lux * lux * lux)
          + (B * lux * lux * lux)
          + (C * lux * lux)
          + (D * lux);
    light_debug("Lux after correction = %"PRIu64, lux);
    if (lux > UINT32_MAX)
    {
        light_debug("Cannot downsize the lux.");
        return false;
    }
    *lux_corrected = (uint32_t)lux;
    return true;
}
#else
static bool _veml7700_conv(uint32_t* lux_dt, uint16_t counts)
{
    /**
     Coeffients calculated with a OSM board and cover.
     */
    float inter_val = counts * _veml7700_ctx.resolution_scaled  / VEML7700_RES_SCALE;
    // Same values as above, but multiplied by +6.2607E+00 as devtank offset
    const float A = +3.7649E-12f;
    const float B = -5.8803E-08f;
    const float C = +5.1017E-04f;
    const float D = +6.2751E+00f;
    inter_val =   (A * inter_val * inter_val * inter_val * inter_val)
                + (B * inter_val * inter_val * inter_val)
                + (C * inter_val * inter_val)
                + (D * inter_val);
    *lux_dt = (uint32_t)inter_val;
    light_debug("Lux after dt correction = %"PRIu32, *lux_dt);
    return true;
}
#endif


static bool _veml7700_increase_integration_time(void)
{
    if (_veml7700_ctx.int_index + 1 == VEML7700_INT_COUNT)
    {
        light_debug("Cannot increase the integration time any more.");
        return false;
    }
    _veml7700_ctx.int_index++;
    light_debug("Increasing the integration time. (mode = %"PRIu8")", _veml7700_ctx.int_index);
    return true;
}


static bool _veml7700_increase_gain(void)
{
    light_debug("Increasing the gain.");
    if (_veml7700_ctx.gain_index + 1 == VEML7700_GAINS_COUNT)
    {
        light_debug("Cannot increase the gain any more.");
        return false;
    }
    _veml7700_ctx.gain_index++;
    return true;
}


static bool _veml7700_increase_settings(void)
{
    if (!_veml7700_increase_gain())
    {
        if (!_veml7700_increase_integration_time())
        {
            light_debug("Cannot increase count any more.");
            return false;
        }
        _veml7700_ctx.gain_index = 0;
    }
    return true;
}


static bool _veml7700_get_counts_begin(void)
{
    if (!_veml7700_set_config())
    {
        return false;
    }
    if (!_veml7700_turn_on())
    {
        return false;
    }
    return true;
}


static bool _veml7700_get_counts_collect(uint16_t* counts)
{
    *counts = _veml7700_read_als();
    if (!_veml7700_turn_off())
    {
        return false;
    }
    return true;
}


static bool _veml7700_check_state(void)
{
    bool r = since_boot_delta(get_since_boot_ms(), _veml7700_time.start_time) <= VEML7700_MAX_READ_TIME;
    if (!r)
        light_debug("Request timed out.");
    return r;
}


static bool _veml7700_iteration_done(void)
{
    return true;
}


static bool _veml7700_iteration_reading(void)
{
    if (!_veml7700_check_state())
        goto bad_exit;
    if (since_boot_delta(get_since_boot_ms(), _veml7700_state_machine.last_read) > _veml7700_ctx.wait_time)
    {
        uint16_t counts;
        if (!_veml7700_get_counts_collect(&counts))
        {
            light_debug("Could not collect counts.");
            goto bad_exit;
        }
        if (counts > VEML7700_COUNT_LOWER_THRESHOLD)
        {
            _veml7700_time.last_time_taken = since_boot_delta(get_since_boot_ms(), _veml7700_time.start_time);
            _veml7700_state_machine.state = VEML7700_STATE_DONE;
            _veml7700_reading.is_valid = true;
            if (!_veml7700_conv(&_veml7700_reading.lux, counts))
            {
                light_debug("Could not convert light.");
                goto bad_exit;
            }
            return true;
        }
        if (!_veml7700_increase_settings())
        {
            _veml7700_time.last_time_taken = since_boot_delta(get_since_boot_ms(), _veml7700_time.start_time);
            _veml7700_state_machine.state = VEML7700_STATE_DONE;
            _veml7700_reading.is_valid = true;
            if (!_veml7700_conv(&_veml7700_reading.lux, counts))
            {
                light_debug("Could not convert light.");
                goto bad_exit;
            }
            return true;
        }
        _veml7700_state_machine.last_read = get_since_boot_ms();
        if (!_veml7700_get_counts_begin())
        {
            light_debug("Could not restart counts.");
            goto bad_exit;
        }
    }
    return true;

bad_exit:
    _veml7700_turn_off();
    _veml7700_state_machine.state = VEML7700_STATE_OFF;
    return false;
}


static bool _veml7700_iteration_off(void)
{
    return true;
}


static measurements_sensor_state_t _veml7700_iteration(char* name)
{
    switch (_veml7700_state_machine.state)
    {
        case VEML7700_STATE_DONE:
            return _veml7700_iteration_done()    ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR;
        case VEML7700_STATE_READING:
            return _veml7700_iteration_reading() ? MEASUREMENTS_SENSOR_STATE_BUSY    : MEASUREMENTS_SENSOR_STATE_ERROR;
        case VEML7700_STATE_OFF:
            return _veml7700_iteration_off()     ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_sensor_state_t _veml7700_measurements_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = VEML7700_DEFAULT_COLLECT_TIME;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _veml7700_light_measurements_init(char* name, bool in_isolation)
{
    switch (_veml7700_state_machine.state)
    {
        case VEML7700_STATE_OFF:
            break;
        case VEML7700_STATE_READING:
            if (!_veml7700_check_state())
            {
                _veml7700_turn_off();
                _veml7700_state_machine.state = VEML7700_STATE_OFF;
                break;
            }
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case VEML7700_STATE_DONE:
            return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    uint32_t now = get_since_boot_ms();
    _veml7700_time.start_time = now;
    _veml7700_state_machine.state = VEML7700_STATE_READING;
    _veml7700_state_machine.last_read = now;
    _veml7700_reset_ctx();
    return (_veml7700_get_counts_begin() ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR);
}


static measurements_sensor_state_t _veml7700_light_measurements_get(char* name, measurements_reading_t* value)
{
    switch (_veml7700_state_machine.state)
    {
        case VEML7700_STATE_OFF:
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case VEML7700_STATE_READING:
            if (!_veml7700_check_state())
                return MEASUREMENTS_SENSOR_STATE_ERROR;
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case VEML7700_STATE_DONE:
            break;
    }
    _veml7700_state_machine.state = VEML7700_STATE_OFF;
    if (!_veml7700_reading.is_valid)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    _veml7700_reading.is_valid = false;
    light_debug("Final lux = %"PRIu32, _veml7700_reading.lux);
    value->v_i64 = (int64_t)_veml7700_reading.lux;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void veml7700_init(void)
{
}


static measurements_value_type_t _veml7700_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void veml7700_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _veml7700_measurements_collection_time;
    inf->init_cb            = _veml7700_light_measurements_init;
    inf->get_cb             = _veml7700_light_measurements_get;
    inf->iteration_cb       = _veml7700_iteration;
    inf->value_type_cb      = _veml7700_value_type;
}
