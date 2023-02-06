#include <stdlib.h>

#include "sai.h"

#include "log.h"
#include "common.h"


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


static command_response_t _sound_cal_cb(char* args)
{
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    if (index < 1 || index > SAI_NUM_CAL_COEFFS)
    {
        log_out("Index out of range.");
        return COMMAND_RESP_ERR;
    }
    p = skip_space(p);
    float coeff = strtof(p, NULL);
    if (!sai_set_coeff(index-1, coeff))
    {
        log_out("Could not set the coefficient.");
        return COMMAND_RESP_ERR;
    }
    return COMMAND_RESP_OK;
}


struct cmd_link_t* sai_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "cal_sound",    "Set the cal coeffs.",      _sound_cal_cb                 , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
