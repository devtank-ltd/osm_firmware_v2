#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <osm/core/config.h>
#include "pinmap.h"
#include <osm/core/persist_config.h>
#include <osm/core/measurements.h>
#include <osm/core/modbus_measurements.h>
#include <osm/sensors/fw.h>
#include <osm/protocols/protocol.h>
#include <osm/core/update.h>
#include "i2s.h"

void osm_model_post_init(void) {}

void osm_model_sensors_init(void)
{
    i2s_init();
}

/* Return true  if different
 *        false if same      */
bool osm_model_persist_config_cmp(osm_persist_model_config_v1_t* d0, osm_persist_model_config_v1_t* d1)
{
    return !(
        d0 && d1 &&
        d0->mins_interval == d1->mins_interval &&
        !osm_comms_persist_config_cmp(&d0->comms_config, &d1->comms_config));
}

bool osm_model_uart_ring_done_in_process(unsigned uart, osm_ring_buf_t * ring)
{
    return false;
}

bool osm_model_measurements_get_inf(osm_measurements_def_t * def, osm_measurements_data_t* data, osm_measurements_inf_t* inf)
{
    if (!def || !inf)
    {
        osm_measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(osm_measurements_inf_t));
    switch(def->type)
    {
        case OSM_FW_VERSION:      osm_fw_version_inf_init(inf);  break;
        case OSM_CONFIG_REVISION: osm_persist_config_inf_init(inf);  break;
        default:
            osm_log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


bool osm_model_uart_ring_do_out_drain(unsigned uart, osm_ring_buf_t * ring)
{
    return true;
}

void osm_model_measurements_repopulate(void)
{
    osm_measurements_repop_indiv((char*)OSM_MEASUREMENTS_FW_VERSION,           4,  1,  OSM_FW_VERSION      );
    osm_measurements_repop_indiv((char*)OSM_MEASUREMENTS_CONFIG_REVISION,      4,  1,  OSM_CONFIG_REVISION );
}


void osm_model_cmds_add_all(struct osm_cmd_link_t* tail)
{
    tail = osm_persist_config_add_commands(tail);
    tail = osm_measurements_add_commands(tail);
    tail = osm_update_add_commands(tail);
    tail = i2s_add_commands(tail);
}


void osm_model_uarts_setup(void) {}


unsigned osm_model_measurements_add_defaults(osm_measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    osm_measurements_setup_default(&measurements_arr[pos++], (char*)OSM_MEASUREMENTS_FW_VERSION,           4,  1,  OSM_FW_VERSION      );
    osm_measurements_setup_default(&measurements_arr[pos++], (char*)OSM_MEASUREMENTS_CONFIG_REVISION,      4,  1,  OSM_CONFIG_REVISION );
    return pos;
}


void osm_model_persist_config_model_init(osm_persist_model_config_v1_t* model_config)
{
    model_config->mins_interval = OSM_MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
}

void osm_model_main_loop_iterate(void)
{
}


bool __attribute__((weak)) osm_bat_on_battery(bool* on_battery)
{
    /* No battery fitted */
    return false;
}


unsigned osm_ios_get_count(void)
{
    return 0;
}
