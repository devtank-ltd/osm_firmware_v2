#include <string.h>
#include "measurements_mem.h"
#include "measurements.h"
#include "log.h"


void measurements_setup_default(measurements_def_t* def, char* name, uint8_t interval, uint8_t samplecount, measurements_def_type_t type)
{
    strncpy(def->name, name, MEASURE_NAME_NULLED_LEN);
    def->interval    = interval;
    def->samplecount = samplecount;
    def->type        = type;
    def->is_immediate = 0;
}


void measurements_repop_indiv(char* name, uint8_t interval, uint8_t samplecount, measurements_def_type_t type)
{
    measurements_def_t def;
    measurements_setup_default(&def, name, interval, samplecount, type);
    measurements_add(&def);
}


measurements_def_t*  measurements_array_find(measurements_def_t * measurements_arr, char* name)
{
    if (!measurements_arr || !name || strlen(name) > MEASURE_NAME_LEN || !name[0])
        return NULL;

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t * def = &measurements_arr[i];
        if (strncmp(def->name, name, MEASURE_NAME_LEN) == 0)
            return def;
    }

    return NULL;
}
