#include <string.h>
#include <osm/core/measurements_mem.h>
#include <osm/core/measurements.h>
#include <osm/core/log.h>


void osm_measurements_setup_default(osm_measurements_def_t* def, char* name, uint8_t interval, uint8_t samplecount, osm_measurements_def_type_t type)
{
    memcpy(def->name, name, OSM_MEASURE_NAME_NULLED_LEN);
    def->name[OSM_MEASURE_NAME_NULLED_LEN-1] = 0;
    def->interval    = interval;
    def->samplecount = samplecount;
    def->type        = type;
    def->is_immediate = 0;
}


void osm_measurements_repop_indiv(char* name, uint8_t interval, uint8_t samplecount, osm_measurements_def_type_t type)
{
    osm_measurements_def_t def;
    osm_measurements_setup_default(&def, name, interval, samplecount, type);
    osm_measurements_add(&def);
}


osm_measurements_def_t*  osm_measurements_array_find(osm_measurements_def_t * measurements_arr, char* name)
{
    if (!measurements_arr || !name || strlen(name) > OSM_MEASURE_NAME_LEN || !name[0])
        return NULL;

    for (unsigned i = 0; i < OSM_MEASUREMENTS_MAX_NUMBER; i++)
    {
        osm_measurements_def_t * def = &measurements_arr[i];
        if (strncmp(def->name, name, OSM_MEASURE_NAME_LEN) == 0)
            return def;
    }

    return NULL;
}
