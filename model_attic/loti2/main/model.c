#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <osm/core/config.h>
#include "pinmap.h"
#include <osm/core/persist_config.h>
#include <osm/core/measurements.h>
#include <osm/core/modbus_measurements.h>
#include <osm/sensors/fw.h>
#include <osm/sensors/adc_loop.h>
#include <osm/protocols/protocol.h>
#include <osm/core/update.h>
#include <osm/core/common.h>
#include <osm/core/cmd.h>
#include <osm/core/io.h>
#include <osm/core/jdesc.h>

#include "model_config.h"
#include "platform_model.h"

static char debug_line_buffer[CMD_LINELEN];


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
static obj_name_t * ios_names = persist_data.model_config.ios_names;
static obj_name_t * adcs_names = persist_data.model_config.adcs_names;
static unit_name_t * adcs_units = persist_data.model_config.adcs_units;
static osm_adc_coeffs_t * adcs_cals = persist_data.model_config.adcs_cals;
#pragma GCC diagnostic pop

static void _uart_debug_out(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_debug_error(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_debug_write(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_debug_flush(osm_cmd_ctx_t * ctx);

static osm_cmd_ctx_t real_uart_ctx = {.output_cb = _uart_debug_out,
                                      .error_cb = _uart_debug_error,
                                      .write_cb = _uart_debug_write,
                                      .flush_cb = _uart_debug_flush };


static void _uart_debug_out(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_outv(fmt, ap);
}


static void _uart_debug_error(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_errorv(fmt, ap);
}


static void _uart_debug_write(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_writev(REAL_UART, false, NULL, fmt, ap);
}




static void _uart_debug_flush(osm_cmd_ctx_t * ctx)
{
    osm_uart_out_drain(1000, REAL_UART);
}


static const char* osm_model_get_obj_name(const obj_name_t * names, unsigned count, unsigned index)
{
    if (index >= count)
        return NULL;

    return names[index];
}

static bool osm_model_get_obj_index(const obj_name_t * names, unsigned count, const char * name, unsigned * index)
{
    if (!name || !index)
        return false;

    for(unsigned n = 0; n < count; n++)
    {
        if (!strcmp(name, names[n]))
        {
            *index = n;
            return true;
        }
    }
    return false;
}

static bool osm_model_set_obj_name(obj_name_t * names, unsigned count, unsigned index, const char * name)
{
    if (index >= count || !name)
        return false;

    unsigned len = strlen(name) + 1;

    if (len > sizeof(obj_name_t))
        return false;

    memcpy(names[index], name, len);

    return true;
}


void osm_model_post_init(void) {}

void osm_model_sensors_init(void)
{
    osm_ios_init();
    osm_adc_loop_set_interval(persist_data.model_config.adc_interval);
    osm_adc_loop_init();
}

/* Return true  if different
 *        false if same      */
bool osm_model_persist_config_cmp(osm_persist_model_config_v1_t* d0, osm_persist_model_config_v1_t* d1)
{
    unsigned adc_interval = osm_adc_loop_get_interval();
    if (adc_interval != persist_data.model_config.adc_interval)
    {
        /* So this is where we make this persistent.*/
        persist_data.model_config.adc_interval = adc_interval;
        return true;
    }
    return (d0->adc_interval != d1->adc_interval ||
            memcmp(d0->ios_state, d1->ios_state, sizeof(uint16_t) * IOS_COUNT) ||
            memcmp(d0->ios_names, d1->ios_names, sizeof(obj_name_t) * IOS_COUNT) ||
            memcmp(d0->adcs_names, d1->adcs_names, sizeof(obj_name_t) * ADCS_CHANNEL_COUNT) ||
            memcmp(d0->adcs_units, d1->adcs_units, sizeof(unit_name_t) * ADCS_CHANNEL_COUNT) ||
            memcmp(d0->adcs_cals,  d1->adcs_cals,  sizeof(osm_adc_coeffs_t) * ADCS_CHANNEL_COUNT)
            );
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
    tail = osm_ios_add_commands(tail);
    tail = osm_adc_loop_add(tail);
    tail = osm_jdesc_add(tail);
}

void osm_measurements_init(void) {}

void osm_model_uarts_setup(void) {}


unsigned osm_model_measurements_add_defaults(osm_measurements_def_t * measurements_arr)
{
    return 0;
}


void osm_model_persist_config_model_init(osm_persist_model_config_v1_t* model_config)
{
    /*model_config->ios_state will be set in osm_ios_init */
    obj_name_t init_ios_names[IOS_COUNT] = IOS_NAMES;
    memcpy(model_config->ios_names, init_ios_names, sizeof(init_ios_names));

    adc_init_t adcs_init[ADCS_CHANNEL_COUNT] = ADCS_INIT;
    for(unsigned n = 0; n < ADCS_CHANNEL_COUNT; n++)
    {
        if (!osm_model_set_obj_name(model_config->adcs_names,
                                    ADCS_CHANNEL_COUNT,
                                    n,
                                    adcs_init[n].name))
            osm_log_error("Failed to load ADC %u name", n);

        const char * unit = adcs_init[n].unit;
        unsigned len = strlen(unit) + 1;
        if (len > sizeof(unit_name_t))
            osm_log_error("Failed to load ADC %u unit", n);
        else
            memcpy(model_config->adcs_units[n], unit, len);

        memcpy(model_config->adcs_cals[n], adcs_init[n].coeffs, sizeof(osm_adc_coeffs_t));
    }
    model_config->adc_interval = 1000;
}


const char* osm_model_get_io_name(unsigned io)
{
    return osm_model_get_obj_name(ios_names, IOS_COUNT, io);
}


bool osm_model_get_io_index(const char * name, unsigned * io)
{
    return osm_model_get_obj_index(ios_names, IOS_COUNT, name, io);
}


bool osm_model_set_io_name(unsigned io, const char * name)
{
    return osm_model_set_obj_name(ios_names, IOS_COUNT, io, name);
}



bool     osm_model_get_adc_index(const char * name, unsigned * adc)
{
    return osm_model_get_obj_index(adcs_names, ADCS_CHANNEL_COUNT, name, adc);
}


bool    osm_model_set_adc_name(unsigned adc, const char * name)
{
    return osm_model_set_obj_name(adcs_names, ADCS_CHANNEL_COUNT, adc, name);
}


const char* osm_model_get_adc_name(unsigned adc)
{
    return osm_model_get_obj_name(adcs_names, ADCS_CHANNEL_COUNT, adc);
}


const char * osm_model_get_adc_unit(unsigned adc)
{
    if (adc >= ADCS_CHANNEL_COUNT)
        return NULL;
    return adcs_units[adc];
}


bool     osm_model_set_adc_unit(unsigned adc, const char * unit)
{
    if (adc >= ADCS_CHANNEL_COUNT || !unit)
        return false;
    unsigned len = strlen(unit) + 1;
    if (len > sizeof(unit_name_t))
        return false;
    memcpy(adcs_units[adc], unit, len);
    return true;
}


double* osm_model_get_adc_cal(unsigned adc)
{
    if (adc >= ADCS_CHANNEL_COUNT)
        return NULL;
    return adcs_cals[adc];
}


bool osm_model_set_adc_cal(unsigned adc, osm_adc_coeffs_t cal)
{
    if (adc >= ADCS_CHANNEL_COUNT || !cal)
        return false;
    memcpy(adcs_cals[adc], cal, sizeof(osm_adc_coeffs_t));
    return true;
}
