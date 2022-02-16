#pragma once

#include "base_types.h"

#define MEASUREMENTS_MAX_NUMBER                   25

#define MEASUREMENTS_PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_AVERAGED     (uint8_t)0x02

#define MEASUREMENT_PM10_NAME          "PM10"
#define MEASUREMENT_PM25_NAME          "PM25"
#define MEASUREMENT_CURRENT_CLAMP_NAME "CC1"
#define MEASUREMENT_W1_PROBE_NAME      "TMP2"
#define MEASUREMENT_HTU21D_TEMP        "TEMP"
#define MEASUREMENT_HTU21D_HUMI        "HUMI"
#define MEASUREMENT_BATMON_NAME        "BAT"
#define MEASUREMENT_PULSE_COUNT_NAME   "PCNT"
#define MEASUREMENT_LIGHT_NAME         "LGHT"

extern unsigned measurements_add_defaults(measurement_def_t * measurement_arr);

extern measurement_def_t*  measurements_array_find(measurement_def_t * measurement_arr, char* name);
