#include <inttypes.h>

#include "pulsecount.h"


#define PULSECOUNT_COLLECTION_TIME_MS       1000;


void pulsecount_init(void)
{
}


void pulsecount_enable(bool enable)
{
    if (enable)
        pulsecount_init();
}


void pulsecount_log()
{
}


measurements_sensor_state_t pulsecount_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = PULSECOUNT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t pulsecount_begin(char* name)
{
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t pulsecount_get(char* name, measurements_reading_t* value)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


void pulsecount_ack(char* name)
{
}
