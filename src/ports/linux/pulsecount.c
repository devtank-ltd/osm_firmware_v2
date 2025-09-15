#include <inttypes.h>

#include <osm/sensors/pulsecount.h>


#define PULSECOUNT_COLLECTION_TIME_MS       1000;


void osm_pulsecount_init(void)
{
}


void osm_pulsecount_enable(unsigned io, bool enable, osm_io_pupd_t pupd, osm_io_special_t edge)
{
    if (enable)
        osm_pulsecount_init();
}


void pulsecount_log()
{
}


static osm_measurements_sensor_state_t _pulsecount_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = PULSECOUNT_COLLECTION_TIME_MS;
    return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static osm_measurements_sensor_state_t _pulsecount_begin(char* name, bool in_isolation)
{
    return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static osm_measurements_sensor_state_t _pulsecount_get(char* name, osm_measurements_reading_t* value)
{
    return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
}


static void _pulsecount_ack(char* name)
{
}


static osm_measurements_value_type_t _pulsecount_value_type(char* name)
{
    return OSM_MEASUREMENTS_VALUE_TYPE_I64;
}


void     osm_pulsecount_inf_init(osm_measurements_inf_t* inf)
{
    inf->collection_time_cb = _pulsecount_collection_time;
    inf->init_cb            = _pulsecount_begin;
    inf->get_cb             = _pulsecount_get;
    inf->acked_cb           = _pulsecount_ack;
    inf->value_type_cb      = _pulsecount_value_type;
}


struct osm_cmd_link_t* osm_pulsecount_add_commands(struct osm_cmd_link_t* tail)
{
    return tail;
}
