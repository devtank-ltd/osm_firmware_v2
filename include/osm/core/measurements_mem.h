#pragma once

#include <osm/core/base_types.h>

#define OSM_MEASUREMENTS_MAX_NUMBER                   256

#define OSM_MEASUREMENTS_PAYLOAD_VERSION       (uint8_t)0x02
#define OSM_MEASUREMENTS_DATATYPE_SINGLE       (uint8_t)0x01
#define OSM_MEASUREMENTS_DATATYPE_AVERAGED     (uint8_t)0x02

#define OSM_MEASUREMENTS_FW_VERSION             "FW"   /* string - Git SHA1 of firmware */
#define OSM_MEASUREMENTS_CONFIG_REVISION        "CREV" /* int    - How many times config has been changed. */
#define OSM_MEASUREMENTS_PM10_NAME              "PM10" /* float  - Particulate matter, less than 10 micrometres in diameter */
#define OSM_MEASUREMENTS_PM25_NAME              "PM25" /* float  - Particulate matter, less than 2.5 micrometres in diameter */
#define OSM_MEASUREMENTS_CURRENT_CLAMP_1_NAME   "CC1"  /* float  - Electrical current from external Current Clamp / Current Transformer in mA */
#define OSM_MEASUREMENTS_CURRENT_CLAMP_2_NAME   "CC2"  /* float  - Electrical current from external Current Clamp / Current Transformer in mA */
#define OSM_MEASUREMENTS_CURRENT_CLAMP_3_NAME   "CC3"  /* float  - Electrical current from external Current Clamp / Current Transformer in mA */
#define OSM_MEASUREMENTS_W1_PROBE_NAME_1        "TMP2" /* float  - External 1Wire probe temperature. */
#define OSM_MEASUREMENTS_W1_PROBE_NAME_2        "TMP3" /* float  - External 1Wire probe temperature. */
#define OSM_MEASUREMENTS_HTU21D_TEMP            "TEMP" /* float  - HTU21D temperature (RevC and below)*/
#define OSM_MEASUREMENTS_HTU21D_HUMI            "HUMI" /* float  - HTU21D relative humidity (RevC and below)*/
#define OSM_MEASUREMENTS_BATMON_NAME            "BAT"  /* float  - Onboard battery percentage. */
#define OSM_MEASUREMENTS_PULSE_COUNT_NAME_1     "CNT1" /* int    - Pulse count for water or gas flow meters */
#define OSM_MEASUREMENTS_PULSE_COUNT_NAME_2     "CNT2" /* int    - Pulse count for water or gas flow meters */
#define OSM_MEASUREMENTS_LIGHT_NAME             "LGHT" /* int    - Illuminance in Lux */
#define OSM_MEASUREMENTS_SOUND_NAME             "SND"  /* float  - Noise level in Decibels, A weighted (dBA) */
#define OSM_MEASUREMENTS_FTMA_1_NAME            "FTA1" /* float  - OSM_FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define OSM_MEASUREMENTS_FTMA_2_NAME            "FTA2" /* float  - OSM_FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define OSM_MEASUREMENTS_FTMA_3_NAME            "FTA3" /* float  - OSM_FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define OSM_MEASUREMENTS_FTMA_4_NAME            "FTA4" /* float  - OSM_FTMA = Four to Twenty Milli Amps, i.e. 4-20mA*/
#define OSM_MEASUREMENTS_PM1_0_NAME             "PM1"  /* float  - Particulate matter, less than 1 micrometres in diameter */
#define OSM_MEASUREMENTS_PM4_NAME               "PM4"  /* float  - Particulate matter, less than 4 micrometres in diameter */
#define OSM_MEASUREMENTS_REL_HUM_NAME           "HUM2" /* float  - Relative Humanity from OSM_SENxx chip (RevD and above) */
#define OSM_MEASUREMENTS_SEN5x_TEMP_NAME        "TMP5" /* float  - Temperature from OSM_SENxx chip (RevD and above)*/
#define OSM_MEASUREMENTS_VOC_NAME               "VOC"  /* float  - Volatile organic compounds from SENxx. */
#define OSM_MEASUREMENTS_NOX_NAME               "NOX"  /* float  - Nitrogen Oxide from SENxx. */
#define OSM_MEASUREMENTS_CO2_NAME               "CO2"  /* float  - Carbon Dioxide from SENxx. */
#define OSM_MEASUREMENTS_HCHO_NAME              "HCHO" /* float  - Formaldehyde from OSM_SENxx */
#define OSM_MEASUREMENTS_EXAMPLE_RS232_NAME     "R232" /* string - OSM_EXAMPLE_RS232 response from command */
#define OSM_MEASUREMENTS_TMP4718_LOCAL_NAME     "TMP6" /* float  - Temperature on POE/Ethernet module, local */
#define OSM_MEASUREMENTS_TMP4718_REMOTE_NAME    "TMP7" /* float  - Temperature on POE/Ethernet module, remote */

#define OSM_MEASUREMENTS_LEGACY_PULSE_COUNT_NAME "PCNT"

void osm_measurements_setup_default(osm_measurements_def_t* def, char* name, uint8_t interval, uint8_t samplecount, osm_measurements_def_type_t type);
void osm_measurements_repop_indiv(char* name, uint8_t interval, uint8_t samplecount, osm_measurements_def_type_t type);

osm_measurements_def_t*  osm_measurements_array_find(osm_measurements_def_t * measurements_arr, char* name);
