#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include <osm/core/i2c.h>
#include <osm/core/common.h>
#include <osm/core/log.h>
#include "persist_config_header_model.h"
#include <osm/sensors/senxx.h>
#include <osm/core/platform.h>
#include <osm/core/sleep.h>

#include "sen5x_i2c.h"
#include "sen66_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"


#define I2C_SENxx_ADDR                                  0x69
#define SENxx_I2C_TIMEOUT_MS                            10
#define SENxx_COLLECTION_MS                             1000

#define SENxx_MEASUREMENT_SCALE_FACTOR_PM1_0            10
#define SENxx_MEASUREMENT_SCALE_FACTOR_PM2_5            10
#define SENxx_MEASUREMENT_SCALE_FACTOR_PM4              10
#define SENxx_MEASUREMENT_SCALE_FACTOR_PM10             10
#define SENxx_MEASUREMENT_SCALE_FACTOR_REL_HUM          100
#define SENxx_MEASUREMENT_SCALE_FACTOR_TEMP             200
#define SENxx_MEASUREMENT_SCALE_FACTOR_VOC              10
#define SENxx_MEASUREMENT_SCALE_FACTOR_NOX              10
#define SENxx_MEASUREMENT_SCALE_FACTOR_CO2              1
#define SENxx_MEASUREMENT_SCALE_FACTOR_HCHO             1     /* TODO: Check this, data not clear as not released at time of writing */

#define SENxx_NAME_RAW_BUF_SIZ                          48
#define SENxx_NAME_BUF_SIZ                              ((SENxx_NAME_RAW_BUF_SIZ * 2) / 3 + 1)
#define SENxx_WAIT_DELAY                                1000
#define SENxx_WARMUP_TIME_MS                            5000

#define SENxx_FAN_INTERVAL_S                            604800 /* 1 week */


typedef enum
{
    SENxx_MEASUREMENT_PM1_0 = 1,
    SENxx_MEASUREMENT_PM2_5,
    SENxx_MEASUREMENT_PM4,
    SENxx_MEASUREMENT_PM10,
    SENxx_MEASUREMENT_REL_HUM,
    SENxx_MEASUREMENT_TEMP,
    SENxx_MEASUREMENT_VOC,
    SENxx_MEASUREMENT_NOX,
    SENxx_MEASUREMENT_CO2,
    SENxx_MEASUREMENT_HCHO,
    SENxx_MEASUREMENT_LEN,
} senxx_measurement_t;

#define SENxx_MODEL_MEASUREMENT_MASK        0xFFFF
#define SENxx_MODEL_MEASUREMENT_SHIFT_C     8
#define SENxx_MODEL_MEASUREMENT_SHIFT(_m)   (1 << (_m + SENxx_MODEL_MEASUREMENT_SHIFT_C))
#define SENxx_MODEL_MEASUREMENT(_m)         (SENxx_MODEL_MEASUREMENT_SHIFT((_m & SENxx_MODEL_MEASUREMENT_MASK)))
#define SENxx_MODEL_HAS_MEASUREMENT(_m, _r) (((_m >> SENxx_MODEL_MEASUREMENT_SHIFT_C) & SENxx_MODEL_MEASUREMENT_MASK) & (1 << _r))

#define SENxx_MEASUREMENT_PM                            (               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_PM1_0)    |               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_PM2_5)    |               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_PM4)      |               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_PM10)     )

#define SENxx_MEASUREMENT_RH_T                          (               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_REL_HUM)  |               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_TEMP)     )

#define SENxx_MEASUREMENT_VOC_NOX                       (               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_VOC)      |               \
    SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_NOX)      )


typedef enum
{
    SENxx_FAMILY_SEN5x = 1,
    SENxx_FAMILY_SEN6x,
} senxx_family_t;

#define SENxx_MODEL_FAMILY_MASK             0xFF
#define SENxx_MODEL_FAMILY(_f)              (SENxx_MODEL_FAMILY_MASK & _f)
#define SENxx_MODEL_IS_FAMILY(_f, _r)       ((_f & SENxx_MODEL_FAMILY_MASK) == _r)


typedef enum
{
    SENxx_MODEL_NONE   = 0,
    SENxx_MODEL_SEN50  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN5x) | SENxx_MEASUREMENT_PM,
    SENxx_MODEL_SEN54  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN5x) | SENxx_MEASUREMENT_PM | SENxx_MEASUREMENT_RH_T | SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_VOC),
    SENxx_MODEL_SEN55  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN5x) | SENxx_MEASUREMENT_PM | SENxx_MEASUREMENT_RH_T | SENxx_MEASUREMENT_VOC_NOX,
    SENxx_MODEL_SEN60  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN6x) | SENxx_MEASUREMENT_PM,
    SENxx_MODEL_SEN63C = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN6x) | SENxx_MEASUREMENT_PM | SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_CO2),
    SENxx_MODEL_SEN65  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN6x) | SENxx_MEASUREMENT_PM | SENxx_MEASUREMENT_RH_T | SENxx_MEASUREMENT_VOC_NOX,
    SENxx_MODEL_SEN66  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN6x) | SENxx_MEASUREMENT_PM | SENxx_MEASUREMENT_RH_T | SENxx_MEASUREMENT_VOC_NOX | SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_CO2),
    SENxx_MODEL_SEN68  = SENxx_MODEL_FAMILY(SENxx_FAMILY_SEN6x) | SENxx_MEASUREMENT_PM | SENxx_MEASUREMENT_RH_T | SENxx_MEASUREMENT_VOC_NOX | SENxx_MODEL_MEASUREMENT(SENxx_MEASUREMENT_HCHO),
} senxx_model_t;


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
    uint16_t    co2;
    uint16_t    hcho;
    uint32_t    time;
} senxx_readings_t;


static struct
{
    bool                is_reading[SENxx_MEASUREMENT_LEN];
    bool                active;
    senxx_readings_t    last_reading;
    senxx_model_t       model;
} _senxx_ctx =
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
        .co2                        = 0,
        .hcho                       = 0,
        .time                       = 0,
    },
    .model          = SENxx_MODEL_NONE,
};


static bool _senxx_get_meas_from_name(char* name, senxx_measurement_t* meas)
{
    unsigned len = strlen(name);
    if (len > MEASURE_NAME_LEN)
        return false;

    if (strncmp(name, MEASUREMENTS_PM1_0_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_PM1_0;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM25_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_PM2_5;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM4_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_PM4;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_PM10_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_PM10;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_REL_HUM_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_REL_HUM;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_SEN5x_TEMP_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_TEMP;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_VOC_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_VOC;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_NOX_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_NOX;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_CO2_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_CO2;
        return true;
    }
    if (strncmp(name, MEASUREMENTS_HCHO_NAME, len) == 0)
    {
        *meas = SENxx_MEASUREMENT_HCHO;
        return true;
    }

    return false;
}


static bool _senxx_get_val(senxx_measurement_t meas, float* val)
{
    if (!val)
    {
        return false;
    }
    senxx_readings_t* r = &_senxx_ctx.last_reading;
    if (!SENxx_MODEL_HAS_MEASUREMENT(_senxx_ctx.model, meas))
    {
        particulate_debug("Not relevant for this sensor.");
        return false;
    }
    switch (meas)
    {
        case SENxx_MEASUREMENT_PM1_0:
            *val = (float)r->mass_concentration_pm1p0 / SENxx_MEASUREMENT_SCALE_FACTOR_PM1_0;
            break;
        case SENxx_MEASUREMENT_PM2_5:
            *val = (float)r->mass_concentration_pm2p5 / SENxx_MEASUREMENT_SCALE_FACTOR_PM2_5;
            break;
        case SENxx_MEASUREMENT_PM4:
            *val = (float)r->mass_concentration_pm4p0 / SENxx_MEASUREMENT_SCALE_FACTOR_PM4;
            break;
        case SENxx_MEASUREMENT_PM10:
            *val = (float)r->mass_concentration_pm10p0 / SENxx_MEASUREMENT_SCALE_FACTOR_PM10;
            break;
        case SENxx_MEASUREMENT_REL_HUM:
            *val = (float)r->ambient_humidity / SENxx_MEASUREMENT_SCALE_FACTOR_REL_HUM;
            break;
        case SENxx_MEASUREMENT_TEMP:
            *val = (float)r->ambient_temperature / SENxx_MEASUREMENT_SCALE_FACTOR_TEMP;
            break;
        case SENxx_MEASUREMENT_VOC:
            *val = (float)r->voc_index / SENxx_MEASUREMENT_SCALE_FACTOR_VOC;
            break;
        case SENxx_MEASUREMENT_NOX:
            *val = (float)r->nox_index / SENxx_MEASUREMENT_SCALE_FACTOR_NOX;
            break;
        case SENxx_MEASUREMENT_CO2:
            *val = (float)r->co2 / SENxx_MEASUREMENT_SCALE_FACTOR_CO2;
            break;
        case SENxx_MEASUREMENT_HCHO:
            *val = (float)r->hcho / SENxx_MEASUREMENT_SCALE_FACTOR_HCHO;
            break;
        default:
            return false;
    }
    return true;
}


senxx_model_t _senxx_get_product(char* name, unsigned name_size)
{
    int16_t error = -1;
    signed char buf[32];
    uint8_t buf_size = 32;
    if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN6x))
    {
        error = sen66_get_product_name(buf, buf_size);
        if (error)
        {
            particulate_debug("Error executing sen66_get_product_name(): %"PRIu16, error);
            return SENxx_MODEL_NONE;
        }
    }
    else if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN5x))
    {
        error = sen5x_get_product_name((unsigned char*)buf, buf_size);
        if (error)
        {
            particulate_debug("Error executing sen5x_get_product_name(): %"PRIu16, error);
            return SENxx_MODEL_NONE;
        }
    }
    else
    {
        /* Dont know which device it is, try 6x first */
        error = sen66_get_product_name(buf, buf_size);
        if (error)
        {
            uint16_t error_5x = sen5x_get_product_name((unsigned char*)buf, buf_size);
            if (error_5x)
            {
                particulate_debug("Error executing _senxx_get_product_name(): %"PRIu16":%"PRIu16, error, error_5x);
                return SENxx_MODEL_NONE;
            }
        }
    }
    particulate_debug("Product name: %s", buf);
    if (name && name_size)
    {
        unsigned len = buf_size > name_size ? name_size : buf_size;
        strncpy(name, (char*)buf, len);
        name[len-1] = 0;
    }
    if ('S' != buf[0] || 'E' != buf[1] || 'N' != buf[2])
    {
        particulate_debug("Could not parse product name");
        return SENxx_MODEL_NONE;
    }
    char model_number_str[4];
    memcpy(model_number_str, &buf[3], 2);
    model_number_str[3] = 0;
    char* next = NULL;
    uint8_t model_number = strtoul(model_number_str, &next, 10);
    switch (model_number)
    {
        case 50:
            return SENxx_MODEL_SEN50;
        case 54:
            return SENxx_MODEL_SEN54;
        case 55:
            return SENxx_MODEL_SEN55;
        case 60:
            return SENxx_MODEL_SEN60;
        case 63:
            if ('C' == *next)
            {
                return SENxx_MODEL_SEN63C;
            }
            break;
        case 65:
            return SENxx_MODEL_SEN65;
        case 66:
            return SENxx_MODEL_SEN66;
        case 68:
            return SENxx_MODEL_SEN66;
        default:
            break;
    }
    particulate_debug("Could not parse model number: '%.2s'", model_number_str);
    return SENxx_MODEL_NONE;
}


static int16_t _senxx_device_reset(void)
{
    int16_t error = -1;
    if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN6x))
    {
        error = sen66_device_reset();
    }
    else if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN5x))
    {
        error = sen5x_device_reset();
    }
    return error;
}


static int16_t _senxx_start_measurement(void)
{
    int16_t error = -1;
    if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN6x))
    {
        error = sen66_start_continuous_measurement();
    }
    else if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN5x))
    {
        error = sen5x_start_measurement();
    }
    return error;
}


static void _senxx_init(void)
{
    sensirion_i2c_hal_init();
    sen66_init(SEN66_I2C_ADDR_6B);
}


void senxx_init(void)
{
    _senxx_init();
    senxx_model_t model = _senxx_get_product(NULL, 0);
    if (SENxx_MODEL_NONE == model)
    {
        particulate_debug("Model could not be read");
        return;
    }
    _senxx_ctx.model = model;
    int16_t error = _senxx_device_reset();
    if (error)
    {
        particulate_debug("Error executing senxx_device_reset(): %"PRIi16, error);
        return;
    }

    particulate_debug("Starting measurement");
    error = _senxx_start_measurement();
    if (error)
    {
        particulate_debug("Error executing senxx_start_measurement(): %"PRIi16, error);
        return;
    }
    _senxx_ctx.active = true;
}


static int16_t _senxx_read_measured_values(senxx_readings_t* reading)
{
    int16_t error = -1;
    if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN6x))
    {
        error = sen66_read_measured_values_as_integers(
            &reading->mass_concentration_pm1p0,
            &reading->mass_concentration_pm2p5,
            &reading->mass_concentration_pm4p0,
            &reading->mass_concentration_pm10p0,
            &reading->ambient_humidity,
            &reading->ambient_temperature,
            &reading->voc_index,
            &reading->nox_index,
            &reading->co2
            );
    }
    else if (SENxx_MODEL_IS_FAMILY(_senxx_ctx.model, SENxx_FAMILY_SEN5x))
    {
        error = sen5x_read_measured_values(
            &reading->mass_concentration_pm1p0,
            &reading->mass_concentration_pm2p5,
            &reading->mass_concentration_pm4p0,
            &reading->mass_concentration_pm10p0,
            &reading->ambient_humidity,
            &reading->ambient_temperature,
            &reading->voc_index,
            &reading->nox_index
            );
    }
    return error;
}


void senxx_iterate(void)
{
    uint32_t now = get_since_boot_ms();
    if (_senxx_ctx.active)
    {
        if (since_boot_delta(now, _senxx_ctx.last_reading.time) >= SENxx_WAIT_DELAY)
        {
            _senxx_ctx.last_reading.time = now;
            int16_t error = _senxx_read_measured_values(&_senxx_ctx.last_reading);

            if (error)
            {
                particulate_debug("Error executing senxx_read_measured_values(): %"PRIi16, error);
                _senxx_ctx.active = false;
            }
        }
    }
    else if (now < SENxx_WARMUP_TIME_MS && since_boot_delta(now, _senxx_ctx.last_reading.time) >= SENxx_WAIT_DELAY)
    {
        _senxx_ctx.last_reading.time = now;
        senxx_init();
    }
}


static measurements_sensor_state_t _senxx_start(char* name, bool in_isolation)
{
    uint32_t now = get_since_boot_ms();
    if (!_senxx_ctx.active && since_boot_delta(now, _senxx_ctx.last_reading.time) >= SENxx_WAIT_DELAY)
    {
        _senxx_ctx.last_reading.time = now;
        senxx_init();
    }
    return _senxx_ctx.active ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_sensor_state_t _senxx_collect(char* name, measurements_reading_t* val)
{
    if (!name || !val)
    {
        particulate_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    senxx_measurement_t meas;
    if (!_senxx_get_meas_from_name(name, &meas))
    {
        particulate_debug("Couldn't get measurement name from '%s'", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!_senxx_ctx.active)
    {
        particulate_debug("SENxx not active");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    float val_f;
    if (!_senxx_get_val(meas, &val_f))
    {
        particulate_debug("Failed conversion %s.", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    val->v_f32 = to_f32_from_float(val_f);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _senxx_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_FLOAT;
}


static measurements_sensor_state_t _senxx_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = SENxx_COLLECTION_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void senxx_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _senxx_collection_time;
    inf->init_cb            = _senxx_start;
    inf->get_cb             = _senxx_collect;
    inf->value_type_cb      = _senxx_value_type;
}


static command_response_t _senxx_pwr_cb(char* args, cmd_ctx_t * ctx)
{
    uint8_t enable = strtoul(args, NULL, 10);
    platform_hpm_enable(enable);
    if (enable)
    {
        cmd_ctx_out(ctx,"SENxx PWR ENABLED");
    }
    else
    {
        cmd_ctx_out(ctx,"SENxx PWR DISABLED");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _senxx_name_cb(char* args, cmd_ctx_t * ctx)
{
    char product_name[32];
    uint8_t product_name_size = 32;
    senxx_model_t model = _senxx_get_product(product_name, product_name_size);
    if (SENxx_MODEL_NONE == model)
    {
        cmd_ctx_out(ctx,"Error executing _senxx_get_product()");
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx,"Product name: %s\n", product_name);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* senxx_add_commands(struct cmd_link_t* tail)
{

    static struct cmd_link_t cmds[] =
    {
        { "senxx_pwr",      "Control PWR for SENxx",    _senxx_pwr_cb, false , NULL },
        { "senxx_name",     "Get name for SENxx",       _senxx_name_cb, false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
