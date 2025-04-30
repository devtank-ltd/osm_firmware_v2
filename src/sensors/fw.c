#include <osm/sensors/fw.h>
#include <osm/core/log.h>
#include <osm/core/measurements.h>

#define FW_SHA_LEN                          7

_Static_assert(sizeof(GIT_SHA1) - 1 <= FW_SHA_LEN, "Firmware hash is too long.");

static char fw_sha1[FW_SHA_LEN+1] = GIT_SHA1;


static measurements_sensor_state_t _fw_version_get(char* name, measurements_reading_t* value)
{
    if (!value)
    {
        measurements_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_str = fw_sha1;
    value->v_str[FW_SHA_LEN] = 0;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _fw_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_STR;
}


void fw_version_inf_init(measurements_inf_t* inf)
{
    inf->get_cb         = _fw_version_get;
    inf->value_type_cb  = _fw_value_type;
}
