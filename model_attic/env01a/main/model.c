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

void model_post_init(void) {}

void model_sensors_init(void)
{
    ios_init();
    modbus_init();
}


bool model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
    {
        modbus_uart_ring_in_process(ring);
        return true;
    }

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
        case MODBUS:          modbus_inf_init(inf);      break;
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
    if (uart == EXT_UART)
        return modbus_uart_ring_do_out_drain(ring);
    return true;
}

void model_measurements_repopulate(void)
{
    measurements_repop_indiv(MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_repop_indiv(MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
}


void model_cmds_add_all(struct cmd_link_t* tail)
{
    tail = ios_add_commands(tail);
    tail = modbus_add_commands(tail);
    tail = persist_config_add_commands(tail);
    tail = measurements_add_commands(tail);
    tail = protocol_add_commands(tail);
}


bool model_can_io_be_special(unsigned io, io_special_t special)
{
    return false;
}


void model_uarts_setup(void) {}


unsigned model_measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
    return pos;
}


void model_persist_config_model_init(persist_model_config_v1_t* model_config)
{
    model_config->mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
}

void model_main_loop_iterate(void)
{
}

void can_impl_init(void) {}
void can_impl_send_example(void) {}

bool bat_on_battery(void) { return false; }

bool io_watch_enable(unsigned io, bool enabled, io_pupd_t pupd)
{
    return false;
}

void     pulsecount_enable(unsigned io, bool enable, io_pupd_t pupd, io_special_t edge) {}

struct cmd_link_t* pulsecount_add_commands(struct cmd_link_t* tail)
{ return tail; }


comms_type_t comms_identify(void)
{
    return COMMS_TYPE_LW;
}
