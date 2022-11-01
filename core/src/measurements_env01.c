#include <string.h>

#include "measurements.h"
#include "log.h"
#include "config.h"
#include "hpm.h"
#include "cc.h"
#include "bat.h"
#include "modbus_measurements.h"
#include "ds18b20.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "veml7700.h"
#include "sai.h"
#include "fw.h"


bool measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !data || !inf)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(measurements_inf_t));
    switch(def->type)
    {
        case FW_VERSION:
            inf->get_cb             = fw_version_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_STR;
            break;
        case PM10:
            inf->collection_time_cb = hpm_collection_time;
            inf->init_cb            = hpm_init;
            inf->get_cb             = hpm_get_pm10;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case PM25:
            inf->collection_time_cb = hpm_collection_time;
            inf->init_cb            = hpm_init;
            inf->get_cb             = hpm_get_pm25;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case MODBUS:
            inf->collection_time_cb = modbus_measurements_collection_time;
            inf->init_cb            = modbus_measurements_init;
            inf->get_cb             = modbus_measurements_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case CURRENT_CLAMP:
            inf->collection_time_cb = cc_collection_time;
            inf->init_cb            = cc_begin;
            inf->get_cb             = cc_get;
            inf->enable_cb          = cc_enable;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case W1_PROBE:
            inf->collection_time_cb = ds18b20_collection_time;
            inf->init_cb            = ds18b20_measurements_init;
            inf->get_cb             = ds18b20_measurements_collect;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_FLOAT;
            break;
        case HTU21D_TMP:
            inf->collection_time_cb = htu21d_measurements_collection_time;
            inf->init_cb            = htu21d_temp_measurements_init;
            inf->get_cb             = htu21d_temp_measurements_get;
            inf->iteration_cb       = htu21d_measurements_iteration;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case HTU21D_HUM:
            inf->collection_time_cb = htu21d_measurements_collection_time;
            inf->init_cb            = htu21d_humi_measurements_init;
            inf->get_cb             = htu21d_humi_measurements_get;
            inf->iteration_cb       = htu21d_measurements_iteration;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case BAT_MON:
            inf->collection_time_cb = bat_collection_time;
            inf->init_cb            = bat_begin;
            inf->get_cb             = bat_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case PULSE_COUNT:
            inf->collection_time_cb = pulsecount_collection_time;
            inf->init_cb            = pulsecount_begin;
            inf->get_cb             = pulsecount_get;
            inf->acked_cb           = pulsecount_ack;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case LIGHT:
            inf->collection_time_cb = veml7700_measurements_collection_time;
            inf->init_cb            = veml7700_light_measurements_init;
            inf->get_cb             = veml7700_light_measurements_get;
            inf->iteration_cb       = veml7700_iteration;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case SOUND:
            inf->collection_time_cb = sai_collection_time;
            inf->init_cb            = sai_measurements_init;
            inf->get_cb             = sai_measurements_get;
            inf->iteration_cb       = sai_iteration_callback;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
    }
    return true;
}


void measurements_repopulate(void)
{
    measurements_def_t def;
    measurements_setup_default(&def, MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    measurements_add(&def);
}
