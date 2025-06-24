#include <stdlib.h>

#include <osm/sensors/sai.h>

#include <osm/core/log.h>
#include <osm/core/common.h>


void osm_sai_init(void)
{
}


bool osm_sai_set_coeff(uint8_t index, float coeff)
{
    return false;
}


void osm_sai_print_coeffs(osm_cmd_ctx_t * ctx)
{
}

static osm_measurements_sensor_state_t _sai_collection_time(char* name, uint32_t* collection_time)
{
    return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
}


static osm_measurements_sensor_state_t _sai_iteration_callback(char* name)
{
    return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static osm_measurements_sensor_state_t _sai_measurements_init(char* name, bool in_isolation)
{
    return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
}


static osm_measurements_sensor_state_t _sai_measurements_get(char* name, osm_measurements_reading_t* value)
{
    return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
}


static osm_measurements_value_type_t _pulsecount_value_type(char* name)
{
    return OSM_MEASUREMENTS_VALUE_TYPE_I64;
}


void  osm_sai_inf_init(osm_measurements_inf_t* inf)
{
    inf->collection_time_cb = _sai_collection_time;
    inf->init_cb            = _sai_measurements_init;
    inf->get_cb             = _sai_measurements_get;
    inf->iteration_cb       = _sai_iteration_callback;
    inf->value_type_cb      = _pulsecount_value_type;
}


static osm_command_response_t _sound_cal_cb(char* args, osm_cmd_ctx_t * ctx)
{
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    if (index < 1 || index > OSM_SAI_NUM_CAL_COEFFS)
    {
        osm_cmd_ctx_error(ctx,"Index out of range.");
        return OSM_COMMAND_RESP_ERR;
    }
    p = osm_skip_space(p);
    float coeff = strtof(p, NULL);
    if (!osm_sai_set_coeff(index-1, coeff))
    {
        osm_cmd_ctx_error(ctx,"Could not set the coefficient.");
        return OSM_COMMAND_RESP_ERR;
    }
    return OSM_COMMAND_RESP_OK;
}


struct osm_cmd_link_t* osm_sai_add_commands(struct osm_cmd_link_t* tail)
{
    static struct osm_cmd_link_t cmds[] = {{ "cal_sound",    "Set the cal coeffs.",      _sound_cal_cb                 , false , NULL }};
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
