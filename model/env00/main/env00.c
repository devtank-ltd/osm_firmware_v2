#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "pinmap.h"
#include "persist_config.h"
#include "measurements.h"
#include "modbus_measurements.h"
#include "fw.h"
#include "io.h"

void env00_post_init(void) {}

void env00_sensors_init(void)
{
    ios_init();
    modbus_init();
}

void env00_debug_mode_enable_all(void) {}


bool env00_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
    {
        modbus_uart_ring_in_process(ring);
        return true;
    }

    return false;
}

bool env00_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
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
        case FW_VERSION:    fw_version_inf_init(inf);  break;
        case MODBUS:        modbus_inf_init(inf);      break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


bool env00_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
        return modbus_uart_ring_do_out_drain(ring);
    return true;
}

void env00_measurements_repopulate(void)
{
    measurements_repop_indiv(MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
}


void env00_cmds_add_all(struct cmd_link_t* tail)
{
    tail = ios_add_commands(tail);
    tail = modbus_add_commands(tail);
}


bool env00_can_io_be_special(unsigned io, io_special_t special)
{
    return false;
}


void env00_uarts_setup(void) {}


unsigned env00_measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    return pos;
}


bool persistent_init(void) {return false;}

uint32_t persist_get_log_debug_mask(void) { return 0; }

void protocol_system_init(void) {}

bool protocol_get_connected(void)
{
    return false;
}

void protocol_loop_iteration(void) {}


void can_impl_send_example(void) {}


void     pulsecount_enable(unsigned io, bool enable, io_pupd_t pupd, io_special_t edge) {}

struct cmd_link_t* pulsecount_add_commands(struct cmd_link_t* tail)
{ return tail; }
