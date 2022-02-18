#include <string.h>
#include "measurements_mem.h"
#include "log.h"


unsigned measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_def_t * def = &measurements_arr[pos++];

    strncpy(def->name, MEASUREMENTS_PM10_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 5;
    def->type        = PM10;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_PM25_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 5;
    def->type        = PM25;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_CURRENT_CLAMP_1_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 25;
    def->type        = CURRENT_CLAMP;
    def = &measurements_arr[pos++];

    strncpy(def->name, MEASUREMENTS_CURRENT_CLAMP_2_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 25;
    def->type        = CURRENT_CLAMP;
    def = &measurements_arr[pos++];

    strncpy(def->name, MEASUREMENTS_CURRENT_CLAMP_3_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 25;
    def->type        = CURRENT_CLAMP;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_W1_PROBE_NAME, MEASURE_NAME_LEN+1);
    def->interval    = 1;
    def->samplecount = 5;
    def->type = W1_PROBE;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_HTU21D_TEMP, MEASURE_NAME_LEN+1);
    def->samplecount = 2;
    def->interval    = 1;
    def->type        = HTU21D_TMP;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_HTU21D_HUMI, MEASURE_NAME_LEN+1);
    def->samplecount = 2;
    def->interval    = 1;
    def->type        = HTU21D_HUM;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_BATMON_NAME, MEASURE_NAME_LEN+1);
    def->samplecount = 5;
    def->interval    = 1;
    def->type        = BAT_MON;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_PULSE_COUNT_NAME, MEASURE_NAME_LEN+1);
    def->samplecount = 1;
    def->interval    = 1;
    def->type        = PULSE_COUNT;

    def = &measurements_arr[pos++];
    strncpy(def->name, MEASUREMENTS_LIGHT_NAME, MEASURE_NAME_LEN+1);
    def->samplecount = 5;
    def->interval    = 1;
    def->type        = LIGHT;

    return pos;
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
