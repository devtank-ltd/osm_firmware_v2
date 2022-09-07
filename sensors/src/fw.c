#include "fw.h"
#include "log.h"
#include "measurements.h"


#define FW_SHA_LEN                          7


static char fw_sha1[FW_SHA_LEN+1] = GIT_SHA1;


measurements_sensor_state_t fw_version_get(char* name, measurements_reading_t* value)
{
    if (!value)
    {
        measurements_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_str = fw_sha1;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}
