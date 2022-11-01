#include "sai.h"


void sai_init(void)
{
}


bool sai_set_coeff(uint8_t index, float coeff)
{
    return false;
}


void sai_print_coeffs(void)
{
}

measurements_sensor_state_t sai_collection_time(char* name, uint32_t* collection_time)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


measurements_sensor_state_t sai_iteration_callback(char* name)
{
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t sai_measurements_init(char* name, bool in_isolation)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


measurements_sensor_state_t sai_measurements_get(char* name, measurements_reading_t* value)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}
