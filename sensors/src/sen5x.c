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
#include "sen5x.h"
#include "platform.h"
#include "sleep.h"
#include "sen5x_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"


#define I2C_SEN5x_ADDR                                  0x69
#define SEN5x_I2C_TIMEOUT_MS                            10

#define SEN5x_MEASUREMENT_SCALE_FACTOR_PM1_0            10
#define SEN5x_MEASUREMENT_SCALE_FACTOR_PM2_5            10
#define SEN5x_MEASUREMENT_SCALE_FACTOR_PM4              10
#define SEN5x_MEASUREMENT_SCALE_FACTOR_PM10             10
#define SEN5x_MEASUREMENT_SCALE_FACTOR_REL_HUM          100
#define SEN5x_MEASUREMENT_SCALE_FACTOR_TEMP             200
#define SEN5x_MEASUREMENT_SCALE_FACTOR_VOC              10
#define SEN5x_MEASUREMENT_SCALE_FACTOR_NOX              10

#define SEN5x_NAME_RAW_BUF_SIZ                          48
#define SEN5x_NAME_BUF_SIZ                              ((SEN5x_NAME_RAW_BUF_SIZ * 2) / 3 + 1)
#define SEN5x_WAIT_DELAY                                1000

#define SEN5x_FAN_INTERVAL_S                            604800 /* 1 week */

typedef enum
{
    SEN5x_MODEL_NONE,
    SEN5x_MODEL_SEN50,
    SEN5x_MODEL_SEN5x,
    SEN5x_MODEL_SEN55,
} sen5x_model_t;



typedef enum
{
    SEN5x_MEASUREMENT_PM1_0 = 0,
    SEN5x_MEASUREMENT_PM2_5,
    SEN5x_MEASUREMENT_PM4,
    SEN5x_MEASUREMENT_PM10,
    SEN5x_MEASUREMENT_REL_HUM,
    SEN5x_MEASUREMENT_TEMP,
    SEN5x_MEASUREMENT_VOC,
    SEN5x_MEASUREMENT_NOX,
    SEN5x_MEASUREMENT_LEN = SEN5x_MEASUREMENT_NOX + 1,
} sen5x_measurement_t;


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
} sen5x_readings_t;


static struct
{
    bool                is_reading[SEN5x_MEASUREMENT_LEN];
    bool                active;
    sen5x_readings_t    last_reading;
    sen5x_model_t       model;
} _sen5x_ctx =
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
    .model          = SEN5x_MODEL_SEN5x, /* Default model currently used */
};


static bool _sen5x_get_meas_from_name(char* name, sen5x_measurement_t* meas)
{
    unsigned len = strlen(name);
    if (len > MEASURE_NAME_LEN)
        return false;

    if (strncmp(name, MEASUREMENTS_PM1_0_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_PM1_0;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM25_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_PM2_5;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM4_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_PM4;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM10_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_PM10;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_REL_HUM_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_REL_HUM;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_SEN5x_TEMP_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_TEMP;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_VOC_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_VOC;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_NOX_NAME, len) == 0)
    {
        *meas = SEN5x_MEASUREMENT_NOX;
        return true;
    }

    return false;
}


static bool _sen5x_get_val(sen5x_measurement_t meas, float* val)
{
    if (!val)
    {
        return false;
    }
    sen5x_readings_t* r = &_sen5x_ctx.last_reading;
    switch (meas)
    {
        case SEN5x_MEASUREMENT_PM1_0:
            *val = (float)r->mass_concentration_pm1p0 / SEN5x_MEASUREMENT_SCALE_FACTOR_PM1_0;
            break;
        case SEN5x_MEASUREMENT_PM2_5:
            *val = (float)r->mass_concentration_pm2p5 / SEN5x_MEASUREMENT_SCALE_FACTOR_PM2_5;
            break;
        case SEN5x_MEASUREMENT_PM4:
            *val = (float)r->mass_concentration_pm4p0 / SEN5x_MEASUREMENT_SCALE_FACTOR_PM4;
            break;
        case SEN5x_MEASUREMENT_PM10:
            *val = (float)r->mass_concentration_pm10p0 / SEN5x_MEASUREMENT_SCALE_FACTOR_PM10;
            break;
        case SEN5x_MEASUREMENT_REL_HUM:
            if (SEN5x_MODEL_SEN5x != _sen5x_ctx.model && SEN5x_MODEL_SEN55 != _sen5x_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->ambient_humidity / SEN5x_MEASUREMENT_SCALE_FACTOR_REL_HUM;
            break;
        case SEN5x_MEASUREMENT_TEMP:
            if (SEN5x_MODEL_SEN5x != _sen5x_ctx.model && SEN5x_MODEL_SEN55 != _sen5x_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->ambient_temperature / SEN5x_MEASUREMENT_SCALE_FACTOR_TEMP;
            break;
        case SEN5x_MEASUREMENT_VOC:
            if (SEN5x_MODEL_SEN5x != _sen5x_ctx.model && SEN5x_MODEL_SEN55 != _sen5x_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->voc_index / SEN5x_MEASUREMENT_SCALE_FACTOR_VOC;
            break;
        case SEN5x_MEASUREMENT_NOX:
            if (SEN5x_MODEL_SEN55 != _sen5x_ctx.model)
            {
                particulate_debug("Not relevant for this sensor.");
                return false;
            }
            *val = (float)r->nox_index / SEN5x_MEASUREMENT_SCALE_FACTOR_NOX;
            break;
        default:
            return false;
    }
    return true;
}


void sen5x_init(void)
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
            _sen5x_ctx.model = SEN5x_MODEL_SEN50;
            break;
        case 54:
            _sen5x_ctx.model = SEN5x_MODEL_SEN5x;
            break;
        case 55:
            _sen5x_ctx.model = SEN5x_MODEL_SEN55;
            break;
        default:
            particulate_debug("Could not parse model number: '%.2s'", model_number_str);
            break;
    }

    particulate_debug("Setting the fan interval");
    error = sen5x_set_fan_auto_cleaning_interval(SEN5x_FAN_INTERVAL_S);
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
    _sen5x_ctx.active = true;
}


void sen5x_iterate(void)
{
    if (_sen5x_ctx.active)
    {
        uint32_t now = get_since_boot_ms();
        if (since_boot_delta(now, _sen5x_ctx.last_reading.time) >= SEN5x_WAIT_DELAY)
        {
            _sen5x_ctx.last_reading.time = now;
            int16_t error;
            error = sen5x_read_measured_values(
                &_sen5x_ctx.last_reading.mass_concentration_pm1p0,
                &_sen5x_ctx.last_reading.mass_concentration_pm2p5,
                &_sen5x_ctx.last_reading.mass_concentration_pm4p0,
                &_sen5x_ctx.last_reading.mass_concentration_pm10p0,
                &_sen5x_ctx.last_reading.ambient_humidity,
                &_sen5x_ctx.last_reading.ambient_temperature,
                &_sen5x_ctx.last_reading.voc_index,
                &_sen5x_ctx.last_reading.nox_index
                );

            if (error)
            {
                particulate_debug("Error executing sen5x_read_measured_values(): %"PRIi16, error);
                _sen5x_ctx.active = false;
            }
        }
    }
}


static measurements_sensor_state_t _sen5x_collect(char* name, measurements_reading_t* val)
{
    if (!name || !val)
    {
        particulate_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    sen5x_measurement_t meas;
    if (!_sen5x_get_meas_from_name(name, &meas))
    {
        particulate_debug("Couldn't get measurement name from '%s'", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!_sen5x_ctx.active)
    {
        particulate_debug("SEN5x not active");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    float val_f;
    if (!_sen5x_get_val(meas, &val_f))
    {
        particulate_debug("Failed conversion %s.", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    val->v_f32 = to_f32_from_float(val_f);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _sen5x_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_FLOAT;
}


void sen5x_inf_init(measurements_inf_t* inf)
{
    inf->get_cb             = _sen5x_collect;
    inf->value_type_cb      = _sen5x_value_type;
}


static command_response_t _sen5x_pwr_cb(char* args, cmd_ctx_t * ctx)
{
    uint8_t enable = strtoul(args, NULL, 10);
    platform_hpm_enable(enable);
    if (enable)
    {
        cmd_ctx_out(ctx,"SEN5x PWR ENABLED");
    }
    else
    {
        cmd_ctx_out(ctx,"SEN5x PWR DISABLED");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _sen5x_dbg_cb(char* args, cmd_ctx_t * ctx)
{
    int16_t error = sen5x_start_measurement();
    if (error)
    {
        particulate_debug("Error executing sen5x_start_measurement(): %"PRIi16, error);
        return COMMAND_RESP_ERR;
    }
    return COMMAND_RESP_OK;
}


static command_response_t _sen5x_name_cb(char* args, cmd_ctx_t * ctx)
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


struct cmd_link_t* sen5x_add_commands(struct cmd_link_t* tail)
{

    static struct cmd_link_t cmds[] =
    {
        { "sen5x_pwr",      "Control PWR for SEN5x",    _sen5x_pwr_cb, false , NULL },
        { "sen5x_dbg",      "Debug SEN5x",              _sen5x_dbg_cb, false , NULL },
        { "sen5x_name",     "Get name for SEN5x",       _sen5x_name_cb, false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
