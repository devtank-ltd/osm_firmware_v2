#include "fw.h"
#include "log.h"
#include "measurements.h"


#define FW_SHA_LEN                          7


static char fw_sha1[FW_SHA_LEN+1] = GIT_SHA1;


static measurements_sensor_state_t _fw_version_get(char* name, measurements_reading_t* value)
{
    if (!value)
    {
        measurements_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_str = fw_sha1;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void fw_version_inf_init(measurements_inf_t* inf)
{
    inf->get_cb     = _fw_version_get;
    inf->value_type = MEASUREMENTS_VALUE_TYPE_STR;
}
