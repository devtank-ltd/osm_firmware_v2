#include <string.h>
#include "measurements_mem.h"
#include "log.h"


unsigned measurements_add_defaults(measurement_def_t * measurement_arr)
{
    if (!measurement_arr)
        return 0;
    unsigned pos = 0;
    measurement_def_t * def = &measurement_arr[pos++];

    strncpy(def->name, MEASUREMENT_PM10_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 5;
    def->type        = PM10;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_PM25_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 5;
    def->type        = PM25;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_CURRENT_CLAMP_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 25;
    def->type        = CURRENT_CLAMP;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_W1_PROBE_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 5;
    def->type = W1_PROBE;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_HTU21D_TEMP, MEASURE_NAME_LEN+1);
    def->samplecount = 2;
    def->interval    = 1;
    def->type        = HTU21D_TMP;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_HTU21D_HUMI, MEASURE_NAME_LEN+1);
    def->samplecount = 2;
    def->interval    = 1;
    def->type        = HTU21D_HUM;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_BATMON_NAME, MEASURE_NAME_LEN+1);
    def->samplecount = 5;
    def->interval    = 1;
    def->type        = BAT_MON;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_PULSE_COUNT_NAME, MEASURE_NAME_LEN+1);
    def->samplecount = 1;
    def->interval    = 1;
    def->type        = PULSE_COUNT;

    def = &measurement_arr[pos++];
    strncpy(def->name, MEASUREMENT_LIGHT_NAME, MEASURE_NAME_LEN+1);
    def->samplecount = 5;
    def->interval    = 1;
    def->type        = LIGHT;

    return pos;
}


measurement_def_t*  measurements_array_find(measurement_def_t * measurement_arr, char* name)
{
    if (!measurement_arr || !name || strlen(name) > MEASURE_NAME_LEN || !name[0])
        return NULL;

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_t * def = &measurement_arr[i];
        if (strncmp(def->name, name, MEASURE_NAME_LEN) == 0)
            return def;
    }

    return NULL;
}
