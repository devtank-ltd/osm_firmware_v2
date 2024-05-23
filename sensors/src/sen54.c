#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "base_types.h"
#include "measurements.h"
#include "i2c.h"
#include "common.h"
#include "log.h"
#include "persist_config_header_model.h"
#include "sen54.h"
#include "platform.h"
#include "sleep.h"
#include "sen5x_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"


#define I2C_SEN54_ADDR                                  0x69
#define SEN54_I2C_TIMEOUT_MS                            10

#define SEN54_MEASUREMENT_SCALE_FACTOR_PM1_0            10
#define SEN54_MEASUREMENT_SCALE_FACTOR_PM2_5            10
#define SEN54_MEASUREMENT_SCALE_FACTOR_PM4              10
#define SEN54_MEASUREMENT_SCALE_FACTOR_PM10             10
#define SEN54_MEASUREMENT_SCALE_FACTOR_REL_HUM          100
#define SEN54_MEASUREMENT_SCALE_FACTOR_TEMP             200
#define SEN54_MEASUREMENT_SCALE_FACTOR_VOC              10
#define SEN54_MEASUREMENT_SCALE_FACTOR_NOX              10

#define SEN54_NAME_RAW_BUF_SIZ                          48
#define SEN54_NAME_BUF_SIZ                              ((SEN54_NAME_RAW_BUF_SIZ * 2) / 3 + 1)
#define SEN54_WAIT_DELAY                                1000

#define SEN54_FAN_INTERVAL_S                            60

typedef enum
{
    SEN54_MODEL_NONE,
    SEN54_MODEL_SEN50,
    SEN54_MODEL_SEN54,
    SEN54_MODEL_SEN55,
} sen54_model_t;



typedef enum
{
    SEN54_MEASUREMENT_PM1_0 = 0,
    SEN54_MEASUREMENT_PM2_5,
    SEN54_MEASUREMENT_PM4,
    SEN54_MEASUREMENT_PM10,
    SEN54_MEASUREMENT_REL_HUM,
    SEN54_MEASUREMENT_TEMP,
    SEN54_MEASUREMENT_VOC,
    SEN54_MEASUREMENT_NOX,
    SEN54_MEASUREMENT_LEN = SEN54_MEASUREMENT_NOX + 1,
} sen54_measurement_t;


typedef struct
{
    uint16_t    mass_concentration_pm1p0;
    uint16_t    mass_concentration_pm2p5;
    uint16_t    mass_concentration_pm4p0;
    uint16_t    mass_concentration_pm10p0;
    int16_t     ambient_humidity;
    int16_t     ambient_temperature;
    int16_t     voc_index;
    int16_t     nox_index;
    uint32_t    time;
} sen54_readings_t;


static struct
{
    bool                is_reading[SEN54_MEASUREMENT_LEN];
    bool                active;
    sen54_readings_t    last_reading;
    sen54_model_t       model;
} _sen54_ctx =
{
    .is_reading     = {false},
    .active         = false,
    .last_reading   =
    {
        .mass_concentration_pm1p0   = 0,
        .mass_concentration_pm2p5   = 0,
        .mass_concentration_pm4p0   = 0,
        .mass_concentration_pm10p0  = 0,
        .ambient_humidity           = 0,
        .ambient_temperature        = 0,
        .voc_index                  = 0,
        .nox_index                  = 0,
        .time                       = 0,
    },
    .model          = SEN54_MODEL_SEN54, /* Default model currently used */
};


static bool _sen54_get_meas_from_name(char* name, sen54_measurement_t* meas)
{
    unsigned len = strlen(name);
    if (len > MEASURE_NAME_LEN)
        return false;

    if (strncmp(name, MEASUREMENTS_PM1_0_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_PM1_0;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM25_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_PM2_5;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM4_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_PM4;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM10_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_PM10;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_REL_HUM_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_REL_HUM;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_SEN54_TEMP_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_TEMP;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_VOC_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_VOC;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_NOX_NAME, len) == 0)
    {
        *meas = SEN54_MEASUREMENT_NOX;
        return true;
    }

    return false;
}


static bool _sen54_get_val(sen54_measurement_t meas, float* val)
{
    if (!val)
    {
        return false;
    }
    sen54_readings_t* r = &_sen54_ctx.last_reading;
    switch (meas)
    {
        case SEN54_MEASUREMENT_PM1_0:
            *val = (float)r->mass_concentration_pm1p0 / SEN54_MEASUREMENT_SCALE_FACTOR_PM1_0;
            break;
        case SEN54_MEASUREMENT_PM2_5:
            *val = (float)r->mass_concentration_pm2p5 / SEN54_MEASUREMENT_SCALE_FACTOR_PM2_5;
            break;
        case SEN54_MEASUREMENT_PM4:
            *val = (float)r->mass_concentration_pm4p0 / SEN54_MEASUREMENT_SCALE_FACTOR_PM4;
            break;
        case SEN54_MEASUREMENT_PM10:
            *val = (float)r->mass_concentration_pm10p0 / SEN54_MEASUREMENT_SCALE_FACTOR_PM10;
            break;
        case SEN54_MEASUREMENT_REL_HUM:
            if (SEN54_MODEL_SEN54 != _sen54_ctx.model && SEN54_MODEL_SEN55 != _sen54_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->ambient_humidity / SEN54_MEASUREMENT_SCALE_FACTOR_REL_HUM;
            break;
        case SEN54_MEASUREMENT_TEMP:
            if (SEN54_MODEL_SEN54 != _sen54_ctx.model && SEN54_MODEL_SEN55 != _sen54_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->ambient_temperature / SEN54_MEASUREMENT_SCALE_FACTOR_TEMP;
            break;
        case SEN54_MEASUREMENT_VOC:
            if (SEN54_MODEL_SEN54 != _sen54_ctx.model && SEN54_MODEL_SEN55 != _sen54_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->voc_index / SEN54_MEASUREMENT_SCALE_FACTOR_VOC;
            break;
        case SEN54_MEASUREMENT_NOX:
            if (SEN54_MODEL_SEN55 != _sen54_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->nox_index / SEN54_MEASUREMENT_SCALE_FACTOR_NOX;
            break;
        default:
            return false;
    }
    return true;
}


void sen54_init(void)
{
    sensirion_i2c_hal_init();
    int16_t error = sen5x_device_reset();
    if (error)
    {
        particulate_debug("Error executing sen5x_device_reset(): %"PRIi16, error);
        return;
    }
    unsigned char buf[32];
    uint8_t buf_size = 32;
    error = sen5x_get_serial_number(buf, buf_size);
    if (error)
    {
        particulate_debug("Error executing sen5x_get_serial_number(): %"PRIi16, error);
        return;
    }
    particulate_debug("Serial number: %s", buf);

    error = sen5x_get_product_name(buf, buf_size);
    if (error)
    {
        particulate_debug("Error executing sen5x_get_product_name(): %"PRIu16, error);
        return;
    }
    particulate_debug("Product name: %s", buf);
    if ('S' != buf[0] || 'E' != buf[1] || 'N' != buf[2])
    {
        particulate_debug("Could not parse product name");
        return;
    }

    char model_number_str[3];
    memcpy(model_number_str, &buf[3], 2);
    model_number_str[2] = 0;
    uint8_t model_number = strtoul(model_number_str, NULL, 10);
    switch (model_number)
    {
        case 50:
            _sen54_ctx.model = SEN54_MODEL_SEN50;
            break;
        case 54:
            _sen54_ctx.model = SEN54_MODEL_SEN54;
            break;
        case 55:
            _sen54_ctx.model = SEN54_MODEL_SEN55;
            break;
        default:
            particulate_debug("Could not parse model number: '%.2s'", model_number_str);
            break;
    }

    particulate_debug("Setting the fan interval");
    error = sen5x_set_fan_auto_cleaning_interval(SEN54_FAN_INTERVAL_S);
    if (error)
    {
        particulate_debug("Error executing sen5x_set_fan_auto_cleaning_interval(): %"PRIi16, error);
        return;
    }

    particulate_debug("Starting measurement");
    error = sen5x_start_measurement();
    if (error)
    {
        particulate_debug("Error executing sen5x_start_measurement(): %"PRIi16, error);
        return;
    }
    _sen54_ctx.active = true;
}


void sen54_iterate(void)
{
    uint32_t now = get_since_boot_ms();
    if (since_boot_delta(now, _sen54_ctx.last_reading.time) >= SEN54_WAIT_DELAY)
    {
        _sen54_ctx.last_reading.time = now;
        int16_t error;
        if (!_sen54_ctx.active)
        {
            sen54_init();
        }
        else
        {
            error = sen5x_read_measured_values(
                &_sen54_ctx.last_reading.mass_concentration_pm1p0,
                &_sen54_ctx.last_reading.mass_concentration_pm2p5,
                &_sen54_ctx.last_reading.mass_concentration_pm4p0,
                &_sen54_ctx.last_reading.mass_concentration_pm10p0,
                &_sen54_ctx.last_reading.ambient_humidity,
                &_sen54_ctx.last_reading.ambient_temperature,
                &_sen54_ctx.last_reading.voc_index,
                &_sen54_ctx.last_reading.nox_index
                );

            if (error)
            {
                particulate_debug("Error executing sen5x_read_measured_values(): %"PRIi16, error);
                _sen54_ctx.active = false;
            }
        }
    }
}


static measurements_sensor_state_t _sen54_collect(char* name, measurements_reading_t* val)
{
    if (!name || !val)
    {
        particulate_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    sen54_measurement_t meas;
    if (!_sen54_get_meas_from_name(name, &meas))
    {
        particulate_debug("Couldn't get measurement name from '%s'", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!_sen54_ctx.active)
    {
        particulate_debug("SEN54 not active");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    float val_f;
    if (!_sen54_get_val(meas, &val_f))
    {
        particulate_debug("Failed conversion %s.", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    val->v_f32 = to_f32_from_float(val_f);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _sen54_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_FLOAT;
}


void sen54_inf_init(measurements_inf_t* inf)
{
    inf->get_cb             = _sen54_collect;
    inf->value_type_cb      = _sen54_value_type;
}


static command_response_t _sen54_pwr_cb(char* args, cmd_ctx_t * ctx)
{
    uint8_t enable = strtoul(args, NULL, 10);
    platform_hpm_enable(enable);
    if (enable)
    {
        cmd_ctx_out(ctx,"SEN54 PWR ENABLED");
    }
    else
    {
        cmd_ctx_out(ctx,"SEN54 PWR DISABLED");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _sen54_dbg_cb(char* args, cmd_ctx_t * ctx)
{
    int16_t error = sen5x_start_measurement();
    if (error)
    {
        particulate_debug("Error executing sen5x_start_measurement(): %"PRIi16, error);
        return COMMAND_RESP_ERR;
    }
    return COMMAND_RESP_OK;
}


static command_response_t _sen54_name_cb(char* args, cmd_ctx_t * ctx)
{
    unsigned char product_name[32];
    uint8_t product_name_size = 32;
    int16_t error = sen5x_get_product_name(product_name, product_name_size);
    if (error)
    {
        cmd_ctx_out(ctx,"Error executing sen5x_get_product_name(): %"PRIu16, error);
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx,"Product name: %s\n", product_name);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* sen54_add_commands(struct cmd_link_t* tail)
{

    static struct cmd_link_t cmds[] =
    {
        { "sen54_pwr",      "Control PWR for SEN54",    _sen54_pwr_cb, false , NULL },
        { "sen54_dbg",      "Debug SEN54",              _sen54_dbg_cb, false , NULL },
        { "sen54_name",     "Get name for SEN54",       _sen54_name_cb, false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
