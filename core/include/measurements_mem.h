#pragma once

#include "base_types.h"

#define MEASUREMENTS_MAX_NUMBER                   25

#define MEASUREMENTS_PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_AVERAGED     (uint8_t)0x02

#define MEASUREMENTS_FW_VERSION             "FW"
#define MEASUREMENTS_PM10_NAME              "PM10"
#define MEASUREMENTS_PM25_NAME              "PM25"
#define MEASUREMENTS_CURRENT_CLAMP_1_NAME   "CC1"
#define MEASUREMENTS_CURRENT_CLAMP_2_NAME   "CC2"
#define MEASUREMENTS_CURRENT_CLAMP_3_NAME   "CC3"
#define MEASUREMENTS_W1_PROBE_NAME_1        "TMP2"
#define MEASUREMENTS_W1_PROBE_NAME_2        "TMP3"
#define MEASUREMENTS_HTU21D_TEMP            "TEMP"
#define MEASUREMENTS_HTU21D_HUMI            "HUMI"
#define MEASUREMENTS_BATMON_NAME            "BAT"
#define MEASUREMENTS_PULSE_COUNT_NAME_1     "CNT1"
#define MEASUREMENTS_PULSE_COUNT_NAME_2     "CNT2"
#define MEASUREMENTS_LIGHT_NAME             "LGHT"
#define MEASUREMENTS_SOUND_NAME             "SND"

#define MEASUREMENTS_LEGACY_PULSE_COUNT_NAME "PCNT"


extern unsigned measurements_add_defaults(measurements_def_t * measurements_arr);
extern void measurements_setup_default(measurements_def_t* def, char* name, uint8_t interval, uint8_t samplecount, measurements_def_type_t type);

extern measurements_def_t*  measurements_array_find(measurements_def_t * measurements_arr, char* name);
