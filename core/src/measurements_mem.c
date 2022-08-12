#include <string.h>
#include "measurements_mem.h"
#include "log.h"


void measurements_setup_default(measurements_def_t* def, char* name, uint8_t interval, uint8_t samplecount, measurements_def_type_t type)
{
    strncpy(def->name, name, MEASURE_NAME_NULLED_LEN);
    def->interval    = interval;
    def->samplecount = samplecount;
    def->type        = type;
}


unsigned measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
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
