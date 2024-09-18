#pragma once

#include "base_types.h"

#define MEASUREMENTS_MAX_NUMBER                   256

#define MEASUREMENTS_PAYLOAD_VERSION       (uint8_t)0x02
#define MEASUREMENTS_DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS_DATATYPE_AVERAGED     (uint8_t)0x02

#define MEASUREMENTS_FW_VERSION             "FW"   /* string - Git SHA1 of firmware */
#define MEASUREMENTS_CONFIG_REVISION        "CREV" /* int    - How many times config has been changed. */
#define MEASUREMENTS_PM10_NAME              "PM10" /* float  - Particulate matter, less than 10 micrometres in diameter */
#define MEASUREMENTS_PM25_NAME              "PM25" /* float  - Particulate matter, less than 2.5 micrometres in diameter */
#define MEASUREMENTS_CURRENT_CLAMP_1_NAME   "CC1"  /* float  - Electrical current from external Current Clamp / Current Transformer in mA */
#define MEASUREMENTS_CURRENT_CLAMP_2_NAME   "CC2"  /* float  - Electrical current from external Current Clamp / Current Transformer in mA */
#define MEASUREMENTS_CURRENT_CLAMP_3_NAME   "CC3"  /* float  - Electrical current from external Current Clamp / Current Transformer in mA */
#define MEASUREMENTS_W1_PROBE_NAME_1        "TMP2" /* float  - External 1Wire probe temperature. */
#define MEASUREMENTS_W1_PROBE_NAME_2        "TMP3" /* float  - External 1Wire probe temperature. */
#define MEASUREMENTS_HTU21D_TEMP            "TEMP" /* float  - HTU21D temperature (RevC and below)*/
#define MEASUREMENTS_HTU21D_HUMI            "HUMI" /* float  - HTU21D relative humidity (RevC and below)*/
#define MEASUREMENTS_BATMON_NAME            "BAT"  /* float  - Onboard battery percentage. */
#define MEASUREMENTS_PULSE_COUNT_NAME_1     "CNT1" /* int    - Pulse count for water or gas flow meters */
#define MEASUREMENTS_PULSE_COUNT_NAME_2     "CNT2" /* int    - Pulse count for water or gas flow meters */
#define MEASUREMENTS_LIGHT_NAME             "LGHT" /* int    - Illuminance in Lux */
#define MEASUREMENTS_SOUND_NAME             "SND"  /* float  - Noise level in Decibels, A weighted (dBA) */
#define MEASUREMENTS_FTMA_1_NAME            "FTA1" /* float  - FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define MEASUREMENTS_FTMA_2_NAME            "FTA2" /* float  - FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define MEASUREMENTS_FTMA_3_NAME            "FTA3" /* float  - FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define MEASUREMENTS_FTMA_4_NAME            "FTA4" /* float  - FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define MEASUREMENTS_PM1_0_NAME             "PM1"  /* float  - Particulate matter, less than 1 micrometres in diameter */
#define MEASUREMENTS_PM4_NAME               "PM4"  /* float  - Particulate matter, less than 4 micrometres in diameter */
#define MEASUREMENTS_REL_HUM_NAME           "HUM2" /* float  - Relative Humanity from SEN5x chip (RevD and above) */
#define MEASUREMENTS_SEN5x_TEMP_NAME        "TMP5" /* float  - Temperature from SEN5x chip (RevD and above)*/
#define MEASUREMENTS_VOC_NAME               "VOC"  /* float  - Volatile organic compounds from SEN5x. */
#define MEASUREMENTS_NOX_NAME               "NOX"  /* float  - Nitrogen Oxide from SEN5x. */
#define MEASUREMENTS_RS232_NAME             "R232" /* string - RS232 response from command */

#define MEASUREMENTS_LEGACY_PULSE_COUNT_NAME "PCNT"

extern void measurements_setup_default(measurements_def_t* def, char* name, uint8_t interval, uint8_t samplecount, measurements_def_type_t type);
extern void measurements_repop_indiv(char* name, uint8_t interval, uint8_t samplecount, measurements_def_type_t type);

extern measurements_def_t*  measurements_array_find(measurements_def_t * measurements_arr, char* name);
