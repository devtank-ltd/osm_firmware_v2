#include <string.h>

#include <osm/core/base_types.h>
#include <osm/core/common.h>
#include <osm/core/log.h>
#include <osm/core/jdesc.h>
#include <osm/core/persist_config.h>
#include <osm/core/io.h>

#include "model.h"

#ifdef ADCS_CHANNEL_COUNT
#include <osm/sensors/adc_loop.h>
#endif


static unsigned _json_written_since_flush = 0;


static void jdesc_out(osm_cmd_ctx_t * ctx, const char * _s)
{
    if (!_json_written_since_flush)
        osm_cmd_ctx_flush(ctx);

    osm_cmd_ctx_write(ctx, _s);

    _json_written_since_flush += strlen(_s);
    if (_json_written_since_flush > 64)
        osm_cmd_ctx_flush(ctx);
}


static osm_command_response_t jdesc_cb(char * args, osm_cmd_ctx_t * ctx)
{
    jdesc_out(ctx, "{");
    jdesc_out(ctx, "\"version\":"STR(JDESC_VERSION)",");
    jdesc_out(ctx, "\"model:\":\""STR(fw_name)"\",");
    jdesc_out(ctx, "\"config\":\""); jdesc_out(ctx, osm_persist_get_human_name()); jdesc_out(ctx, "\",");
    jdesc_out(ctx, "\"serial\":\"TODO\",");
    jdesc_out(ctx, "\"objects\":{");
    jdesc_out(ctx, "\"ios\":{");
    for(unsigned n = 0; n < IOS_COUNT; n++)
    {
        jdesc_out(ctx, "\"");
        jdesc_out(ctx, osm_model_get_io_name(n));
        jdesc_out(ctx, "\":{\"direction\":");
        jdesc_out(ctx, (osm_io_is_input(n))?"\"IN\",":"\"OUT\",");
        jdesc_out(ctx, "\"bias\":\""); jdesc_out(ctx, osm_io_get_pull_str(n)); jdesc_out(ctx, "\"");
        const char * special_name = osm_ios_get_special_name(n);
        if (special_name && special_name[0])
        {
            jdesc_out(ctx, ",");
            jdesc_out(ctx, "\"has_special\":\""); jdesc_out(ctx, special_name); jdesc_out(ctx, "\",");
            jdesc_out(ctx, "\"is_special\":"); jdesc_out(ctx, (osm_io_special_enabled(n))?"1":"0");
        }
        jdesc_out(ctx, "}");
        if ((n + 1) < IOS_COUNT)
            jdesc_out(ctx, ",");
    }
#ifdef ADCS_CHANNEL_COUNT /* Directly exposed ADCs is optional. */
    jdesc_out(ctx, "},\"adcs\":{");
    for(unsigned n = 0; n < ADCS_CHANNEL_COUNT; n++)
    {
        jdesc_out(ctx, "\"");
        jdesc_out(ctx, osm_model_get_adc_name(n));
        const char* unit = osm_model_get_adc_unit(n);
        jdesc_out(ctx, "\":{");
        if (unit)
        {
            jdesc_out(ctx, "\"Unit\":\"");jdesc_out(ctx, unit); jdesc_out(ctx, "\"");
        }
        jdesc_out(ctx, "}");
        if ((n + 1) < ADCS_CHANNEL_COUNT)
            jdesc_out(ctx, ",");
    }
#endif /* ADCS_CHANNEL_COUNT */
/*
    jdesc_out(ctx, "},\"ppss\":{");
    for(unsigned n = 0; n < pulsecount_get_count(); n++)
    {
        jdesc_out(ctx, "\"");
        jdesc_out(ctx, pulsecount_get_name(n));
        jdesc_out(ctx, "\":{}");
        if ((n + 1) < pulsecount_get_count())
            jdesc_out(ctx, ",");
    }
    jdesc_out(ctx, "},\"pwms\":{");
    for(unsigned n = 0; n < PWM_COUNT; n++)
    {
        jdesc_out(ctx, "\"");
        uint32_t freq;
        uint8_t duty;
        char* name = NULL;
        persistent_get_pwm_config(n, &name, &freq, &duty);
        jdesc_out(ctx, name);
        jdesc_out(ctx, "\":{");
        jdesc_out(ctx, "\"frequency\":");
        char buf[32];
        snprintf(buf, 32, "%"PRIu32",\"duty\":%"PRIu8, freq, duty);
        buf[31] = 0;
        jdesc_out(ctx, buf);
        jdesc_out(ctx, "}");
        if ((n + 1) < PWM_COUNT)
            jdesc_out(ctx, ",");
    }
    jdesc_out(ctx, "},\"uarts\":{");
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        jdesc_out(ctx, "\"");
        jdesc_out(ctx, uart_get_name(n));
        jdesc_out(ctx, "\":{");
        unsigned baud;
        uint8_t databits;
        uart_parity_t parity;
        uart_stop_bits_t stop;
        if (uart_get_setup(n, &baud, &databits, &parity, &stop))
            uart_ring_out_printf(CMD_VUART, "\"baud\":%u,\"parity\":\"%c\",\"bits\":%"PRIu8",\"stopbits\":%"PRIu8,
                                            baud, uart_parity_as_char(parity), databits, uart_stop_bits_as_int(stop));
        jdesc_out(ctx, "}");
        if ((n + 1) < UART_CHANNELS_COUNT)
            jdesc_out(ctx, ",");
    }
*/
    jdesc_out(ctx, "}}}\n\r");
    _json_written_since_flush = 0;
    osm_cmd_ctx_flush(ctx);
    return OSM_COMMAND_RESP_OK;
}


struct osm_cmd_link_t* osm_jdesc_add(struct osm_cmd_link_t* tail)
{
    static struct osm_cmd_link_t cmds[] = {{ "jdesc",    "JSON description of interfaces", jdesc_cb, false, NULL },
    };
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
