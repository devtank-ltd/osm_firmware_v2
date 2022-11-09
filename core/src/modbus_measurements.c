#include <string.h>

#include "modbus_measurements.h"
#include "modbus.h"
#include "common.h"

/* On tests it's about 300ms per register @9600 with the RI-F220
 * Max 4 devices with max 16 registers
 * 300 * 16 * 4 = 19200
 */
#define MODBUS_COLLECTION_MS 20000


static measurements_sensor_state_t _modbus_measurements_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = MODBUS_COLLECTION_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}

static measurements_sensor_state_t _modbus_measurements_init(char* name, bool in_isolation)
{
    if (in_isolation)
    {
        if (modbus_has_pending())
        {
            modbus_debug("Unable to get modbus reg in isolation as bus is busy.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        }
    }

    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    return (modbus_start_read(reg) ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR);
}


static measurements_sensor_state_t _modbus_measurements_get(char* name, measurements_reading_t* value)
{
    if (!value)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    modbus_reg_state_t state = modbus_reg_get_state(reg);

    if (state == MB_REG_WAITING)
        return MEASUREMENTS_SENSOR_STATE_BUSY;

    if (state != MB_REG_READY)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    switch(modbus_reg_get_type(reg))
    {
        case MODBUS_REG_TYPE_U16:
        {
            uint16_t v;

            if (!modbus_reg_get_u16(reg, &v))
                return MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_i64 = (int64_t)v;
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        case MODBUS_REG_TYPE_U32:
        {
            uint32_t v;

            if (!modbus_reg_get_u32(reg, &v))
                return MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_i64 = (int64_t)v;
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        case MODBUS_REG_TYPE_FLOAT:
        {
            float v;

            if (!modbus_reg_get_float(reg, &v))
                return MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_f32 = to_f32_from_float(v);
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        default: break;
    }
   return MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_value_type_t _modbus_measurements_value_type(char* name)
{
    modbus_reg_t* reg = modbus_get_reg(name);
    modbus_reg_type_t reg_type = modbus_reg_get_type(reg);
    switch(reg_type)
    {
        case MODBUS_REG_TYPE_FLOAT:
            return MEASUREMENTS_VALUE_TYPE_FLOAT;
        case MODBUS_REG_TYPE_U16:
            return MEASUREMENTS_VALUE_TYPE_I64;
        case MODBUS_REG_TYPE_U32:
            return MEASUREMENTS_VALUE_TYPE_I64;
        default:
            modbus_debug("Unknown modbus register type.");
            break;
    }
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void modbus_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _modbus_measurements_collection_time;
    inf->init_cb            = _modbus_measurements_init;
    inf->get_cb             = _modbus_measurements_get;
    inf->value_type_cb      = _modbus_measurements_value_type;
}


bool modbus_measurement_add(modbus_reg_t * reg)
{
    if (!reg)
        return false;

    measurements_def_t meas_def;

    modbus_reg_get_name(reg, meas_def.name);

    measurements_del(meas_def.name);

    meas_def.name[MODBUS_NAME_LEN] = 0;
    meas_def.samplecount = 1;
    meas_def.interval    = 1;
    meas_def.type        = MODBUS;

    return measurements_add(&meas_def);
}


void modbus_measurement_del_reg(char * name)
{
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return;
    measurements_del(name);
    modbus_reg_del(reg);
}

void modbus_measurement_del_dev(char * dev_name)
{
    modbus_dev_t * dev = modbus_get_device_by_name(dev_name);
    if (!dev)
        return;

    modbus_reg_t * reg = modbus_dev_get_reg(dev, 0);
    while (reg)
    {
        char name[MODBUS_NAME_LEN+1];
        modbus_reg_get_name(reg, name);
        measurements_del(name);
        modbus_reg_del(reg);
        reg = modbus_dev_get_reg(dev, 0);
    }
    modbus_dev_del(dev);
}
