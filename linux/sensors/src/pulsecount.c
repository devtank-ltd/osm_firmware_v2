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


static measurements_sensor_state_t _pulsecount_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = PULSECOUNT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _pulsecount_begin(char* name, bool in_isolation)
{
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _pulsecount_get(char* name, measurements_reading_t* value)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


static void _pulsecount_ack(char* name)
{
}


void     pulsecount_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _pulsecount_collection_time;
    inf->init_cb            = _pulsecount_begin;
    inf->get_cb             = _pulsecount_get;
    inf->acked_cb           = _pulsecount_ack;
    inf->value_type         = MEASUREMENTS_VALUE_TYPE_I64;
}
