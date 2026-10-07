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
#include <osm/core/common.h>
#include <osm/core/cmd.h>

char debug_line_buffer[CMD_LINELEN];

static void _uart_debug_out(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_debug_error(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_debug_flush(osm_cmd_ctx_t * ctx);

static osm_cmd_ctx_t real_uart_ctx = {.output_cb = _uart_debug_out,
                                      .error_cb = _uart_debug_error,
                                      .flush_cb = _uart_debug_flush };


static void _uart_debug_out(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_outv(fmt, ap);
}


static void _uart_debug_error(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_errorv(fmt, ap);
}


static void _uart_debug_flush(osm_cmd_ctx_t * ctx)
{
    osm_uart_out_drain(1000, REAL_UART);
}



void osm_model_post_init(void) {}

void osm_model_sensors_init(void)
{
}

/* Return true  if different
 *        false if same      */
bool osm_model_persist_config_cmp(osm_persist_model_config_v1_t* d0, osm_persist_model_config_v1_t* d1)
{
    return false;
}

bool osm_model_uart_ring_done_in_process(unsigned uart, osm_ring_buf_t * ring)
{
    if (uart == REAL_UART)
    {
        unsigned len = osm_ring_buf_readline(ring, debug_line_buffer, CMD_LINELEN);
        if (len)
            osm_cmds_process(debug_line_buffer, len, &real_uart_ctx);
        return true;
    }
    return false;
}

bool osm_model_measurements_get_inf(osm_measurements_def_t * def, osm_measurements_data_t* data, osm_measurements_inf_t* inf)
{
    return false;
}


bool osm_model_uart_ring_do_out_drain(unsigned uart, osm_ring_buf_t * ring)
{
    return true;
}

void osm_protocol_system_init(void) {}
void osm_protocol_loop_iteration(void) {}
bool osm_protocol_get_connected(void) { return false; }

void osm_model_measurements_repopulate(void) {}
void osm_measurements_loop_iteration(void) {}


void osm_model_cmds_add_all(struct osm_cmd_link_t* tail)
{
    tail = osm_persist_config_add_commands(tail);
    tail = osm_update_add_commands(tail);
}

void osm_measurements_init(void) {}

void osm_model_uarts_setup(void) {}


unsigned osm_model_measurements_add_defaults(osm_measurements_def_t * measurements_arr)
{
    return 0;
}


void osm_model_persist_config_model_init(osm_persist_model_config_v1_t* model_config)
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
