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

static measurements_sensor_state_t _sai_collection_time(char* name, uint32_t* collection_time)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_sensor_state_t _sai_iteration_callback(char* name)
{
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _sai_measurements_init(char* name, bool in_isolation)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_sensor_state_t _sai_measurements_get(char* name, measurements_reading_t* value)
{
    return MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_value_type_t _pulsecount_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void  sai_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _sai_collection_time;
    inf->init_cb            = _sai_measurements_init;
    inf->get_cb             = _sai_measurements_get;
    inf->iteration_cb       = _sai_iteration_callback;
    inf->value_type_cb      = _pulsecount_value_type;
}
