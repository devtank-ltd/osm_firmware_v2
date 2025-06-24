#include <string.h>

#include <osm/core/modbus_measurements.h>
#include <osm/core/modbus.h>
#include <osm/core/common.h>

/* On tests it's about 300ms per register @9600 with the RI-F220
 * Max 4 devices with max 16 registers
 * 300 * 16 * 4 = 19200
 */
#define MODBUS_COLLECTION_MS 4000


static osm_measurements_sensor_state_t _modbus_measurements_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = MODBUS_COLLECTION_MS;
    return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
}

static osm_measurements_sensor_state_t _modbus_measurements_init(char* name, bool in_isolation)
{
    if (in_isolation)
    {
        if (osm_modbus_has_pending())
        {
            osm_modbus_debug("Unable to get modbus reg in isolation as bus is busy.");
            return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
        }
    }

    osm_modbus_reg_t * reg = osm_modbus_get_reg(name);
    if (!reg)
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
    return (osm_modbus_start_read(reg) ? OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS : OSM_MEASUREMENTS_SENSOR_STATE_ERROR);
}


static osm_measurements_sensor_state_t _modbus_measurements_get(char* name, osm_measurements_reading_t* value)
{
    if (!value)
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
    osm_modbus_reg_t * reg = osm_modbus_get_reg(name);
    if (!reg)
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

    osm_modbus_reg_state_t state = osm_modbus_reg_get_state(reg);

    if (state == OSM_MB_REG_WAITING)
        return OSM_MEASUREMENTS_SENSOR_STATE_BUSY;

    if (state != OSM_MB_REG_READY)
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

    switch(osm_modbus_reg_get_type(reg))
    {
        case OSM_MODBUS_REG_TYPE_U16:
        {
            uint16_t v;

            if (!osm_modbus_reg_get_u16(reg, &v))
                return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_i64 = (int64_t)v;
            return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        case OSM_MODBUS_REG_TYPE_I16:
        {
            int16_t v;

            if (!osm_modbus_reg_get_i16(reg, &v))
                return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_i64 = (int64_t)v;
            return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        case OSM_MODBUS_REG_TYPE_U32:
        {
            uint32_t v;

            if (!osm_modbus_reg_get_u32(reg, &v))
                return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_i64 = (int64_t)v;
            return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        case OSM_MODBUS_REG_TYPE_I32:
        {
            int32_t v;

            if (!osm_modbus_reg_get_i32(reg, &v))
                return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_i64 = (int64_t)v;
            return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        case OSM_MODBUS_REG_TYPE_FLOAT:
        {
            float v;

            if (!osm_modbus_reg_get_float(reg, &v))
                return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;

            value->v_f32 = osm_to_f32_from_float(v);
            return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
        default: break;
    }
   return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
}


static osm_measurements_value_type_t _modbus_measurements_value_type(char* name)
{
    osm_modbus_reg_t* reg = osm_modbus_get_reg(name);
    osm_modbus_reg_type_t reg_type = osm_modbus_reg_get_type(reg);
    switch(reg_type)
    {
        case OSM_MODBUS_REG_TYPE_FLOAT:
            return OSM_MEASUREMENTS_VALUE_TYPE_FLOAT;
        case OSM_MODBUS_REG_TYPE_U16:
            return OSM_MEASUREMENTS_VALUE_TYPE_I64;
        case OSM_MODBUS_REG_TYPE_I16:
            return OSM_MEASUREMENTS_VALUE_TYPE_I64;
        case OSM_MODBUS_REG_TYPE_U32:
            return OSM_MEASUREMENTS_VALUE_TYPE_I64;
        case OSM_MODBUS_REG_TYPE_I32:
            return OSM_MEASUREMENTS_VALUE_TYPE_I64;
        default:
            osm_modbus_debug("Unknown modbus register type.");
            break;
    }
    return OSM_MEASUREMENTS_VALUE_TYPE_I64;
}


void osm_modbus_inf_init(osm_measurements_inf_t* inf)
{
    inf->collection_time_cb = _modbus_measurements_collection_time;
    inf->init_cb            = _modbus_measurements_init;
    inf->get_cb             = _modbus_measurements_get;
    inf->value_type_cb      = _modbus_measurements_value_type;
}


bool osm_modbus_measurement_add(osm_modbus_reg_t * reg)
{
    if (!reg)
        return false;

    osm_measurements_def_t meas_def;

    osm_modbus_reg_get_name(reg, meas_def.name);

    osm_measurements_del(meas_def.name);

    meas_def.name[OSM_MODBUS_NAME_LEN] = 0;
    meas_def.samplecount = 1;
    meas_def.interval    = 1;
    meas_def.type        = OSM_MODBUS;
    meas_def.is_immediate = 0;

    return osm_measurements_add(&meas_def);
}


bool osm_modbus_measurement_del_reg(char * name)
{
    osm_modbus_reg_t * reg = osm_modbus_get_reg(name);
    if (!reg)
        return false;
    bool r = osm_measurements_del(name);
    osm_modbus_reg_del(reg);
    return r;
}


static bool _modbus_measurement_del_dev_reg(osm_modbus_reg_t * reg, void * userdata)
{
    char name[OSM_MODBUS_NAME_LEN+1];
    if (osm_modbus_reg_get_name(reg, name))
        osm_measurements_del(name);
    return false;
}


bool osm_modbus_measurement_del_dev(char * dev_name)
{
    osm_modbus_dev_t * dev = osm_modbus_get_device_by_name(dev_name);
    if (!dev)
        return false;

    bool r = osm_modbus_dev_for_each_reg(dev, _modbus_measurement_del_dev_reg, NULL);

    osm_modbus_dev_del(dev);
    return r;
}
