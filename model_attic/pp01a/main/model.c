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

void model_post_init(void) {}

void model_sensors_init(void)
{
}

/* Return true  if different
 *        false if same      */
bool model_persist_config_cmp(persist_model_config_v1_t* d0, persist_model_config_v1_t* d1)
{
    return !(
        d0 && d1 &&
        d0->mins_interval == d1->mins_interval &&
        !comms_persist_config_cmp(&d0->comms_config, &d1->comms_config));
}

bool model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring)
{
    return false;
}

bool model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !inf)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(measurements_inf_t));
    switch(def->type)
    {
        case FW_VERSION:      fw_version_inf_init(inf);  break;
        case CONFIG_REVISION: persist_config_inf_init(inf);  break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


bool model_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring)
{
    return true;
}

void model_measurements_repopulate(void)
{
    measurements_repop_indiv((char*)MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_repop_indiv((char*)MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
}


void model_cmds_add_all(struct cmd_link_t* tail)
{
    tail = persist_config_add_commands(tail);
    tail = measurements_add_commands(tail);
    tail = update_add_commands(tail);
}


void model_uarts_setup(void) {}


unsigned model_measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], (char*)MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], (char*)MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
    return pos;
}


void model_persist_config_model_init(persist_model_config_v1_t* model_config)
{
    model_config->mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
}

void model_main_loop_iterate(void)
{
}


bool __attribute__((weak)) bat_on_battery(bool* on_battery)
{
    /* No battery fitted */
    return false;
}


unsigned ios_get_count(void)
{
    return 0;
}
