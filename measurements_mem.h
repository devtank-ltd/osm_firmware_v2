#pragma once

#include "base_types.h"

#define MEASUREMENTS_MAX_NUMBER                   25

#define MEASUREMENTS_PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_AVERAGED     (uint8_t)0x02

#define MEASUREMENTS_PM10_NAME              "PM10"
#define MEASUREMENTS_PM25_NAME              "PM25"
#define MEASUREMENTS_CURRENT_CLAMP_1_NAME   "CC1"
#define MEASUREMENTS_CURRENT_CLAMP_2_NAME   "CC2"
#define MEASUREMENTS_CURRENT_CLAMP_3_NAME   "CC3"
#define MEASUREMENTS_W1_PROBE_NAME          "TMP2"
#define MEASUREMENTS_HTU21D_TEMP            "TEMP"
#define MEASUREMENTS_HTU21D_HUMI            "HUMI"
#define MEASUREMENTS_BATMON_NAME            "BAT"
#define MEASUREMENTS_PULSE_COUNT_NAME       "PCNT"
#define MEASUREMENTS_LIGHT_NAME             "LGHT"

extern unsigned measurements_add_defaults(measurements_def_t * measurements_arr);

extern measurements_def_t*  measurements_array_find(measurements_def_t * measurements_arr, char* name);
