#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <osm/core/config.h>
#include "pinmap.h"
#include <osm/core/persist_config.h>
#include <osm/core/measurements.h>
#include <osm/core/modbus_measurements.h>
#include <osm/sensors/fw.h>
#include <osm/core/io.h>
#include <osm/protocols/protocol.h>

void osm_model_post_init(void) {}

void osm_model_sensors_init(void)
{
    osm_ios_init();
    osm_modbus_init();
}


bool osm_model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
    {
        osm_modbus_uart_ring_in_process(ring);
        return true;
    }

    return false;
}

bool osm_model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !inf)
    {
        osm_measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(measurements_inf_t));
    switch(def->type)
    {
        case OSM_FW_VERSION:      osm_fw_version_inf_init(inf);  break;
        case OSM_MODBUS:          osm_modbus_inf_init(inf);      break;
        case OSM_CONFIG_REVISION: osm_persist_config_inf_init(inf);  break;
        default:
            osm_log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


bool osm_model_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
        return osm_modbus_uart_ring_do_out_drain(ring);
    return true;
}

void osm_model_measurements_repopulate(void)
{
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_FW_VERSION,           4,  1,  OSM_FW_VERSION      );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_CONFIG_REVISION,      4,  1,  OSM_CONFIG_REVISION );
}


void osm_model_cmds_add_all(struct cmd_link_t* tail)
{
    tail = osm_ios_add_commands(tail);
    tail = osm_modbus_add_commands(tail);
    tail = osm_persist_config_add_commands(tail);
    tail = osm_measurements_add_commands(tail);
    tail = osm_protocol_add_commands(tail);
}


bool osm_model_can_io_be_special(unsigned io, osm_io_special_t special)
{
    return false;
}


void osm_model_uarts_setup(void) {}


unsigned osm_model_measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    osm_measurements_setup_default(&measurements_arr[pos++], OSM_MEASUREMENTS_FW_VERSION,           4,  1,  OSM_FW_VERSION      );
    osm_measurements_setup_default(&measurements_arr[pos++], OSM_MEASUREMENTS_CONFIG_REVISION,      4,  1,  OSM_CONFIG_REVISION );
    return pos;
}


void osm_model_persist_config_model_init(persist_model_config_v1_t* model_config)
{
    model_config->mins_interval = OSM_MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
}

void osm_model_main_loop_iterate(void)
{
}

void osm_can_impl_init(void) {}
void osm_can_impl_send_example(void) {}

bool osm_bat_on_battery(void) { return false; }

bool osm_io_watch_enable(unsigned io, bool enabled, osm_io_pupd_t pupd)
{
    return false;
}

void     osm_pulsecount_enable(unsigned io, bool enable, osm_io_pupd_t pupd, osm_io_special_t edge) {}

struct cmd_link_t* osm_pulsecount_add_commands(struct cmd_link_t* tail)
{ return tail; }


osm_comms_type_t osm_comms_identify(void)
{
    return OSM_COMMS_TYPE_LW;
}
