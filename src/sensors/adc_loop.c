#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <osm/core/common.h>
#include <osm/core/log.h>
#include "model_config.h"
#include "platform_model.h"

#define MAX_MINMAX_COUNT 12

typedef struct
{
    uint32_t total_value; /* Max frequency of 1Mhz for 12 bit ADC*/

    uint32_t count:23;
    uint32_t average_limits:1;
    uint32_t max_min_index:4; /* Max number of the mins. */
    uint32_t min_max_index:4; /* Min number of the maxs. */

    uint16_t max_values[MAX_MINMAX_COUNT];
    uint16_t min_values[MAX_MINMAX_COUNT];
} adc_channel_info_t;


static volatile adc_channel_info_t _adc_channel_info[ADCS_CHANNEL_COUNT] = {{0}};
static volatile adc_channel_info_t _adc_channel_info_cur[ADCS_CHANNEL_COUNT] = {{0}};

static volatile unsigned _collection_ms = 1000;

static volatile uint64_t _last_sample = 0;

static bool _use_calibration = true;


static void _setup_lowest_max(volatile adc_channel_info_t * channel_info)
{
    uint16_t lowest_value = channel_info->max_values[0];
    channel_info->min_max_index = 0;
    for(unsigned n = 1; n < MAX_MINMAX_COUNT; n++)
    {
        if (lowest_value > channel_info->max_values[n])
        {
            lowest_value = channel_info->max_values[n];
            channel_info->min_max_index = n;
        }
    }
}


static void _setup_largest_min(volatile adc_channel_info_t * channel_info)
{
    uint16_t largest_value = channel_info->min_values[0];
    channel_info->max_min_index = 0;
    for(unsigned n = 1; n < MAX_MINMAX_COUNT; n++)
    {
        if (largest_value < channel_info->min_values[n])
        {
            largest_value = channel_info->min_values[n];
            channel_info->max_min_index = n;
        }
    }
}


void osm_adc_loop_channel_add(unsigned channel, uint16_t adc)
{
    volatile adc_channel_info_t * channel_info = &_adc_channel_info[channel];

    if (channel_info->average_limits)
    {
        if (channel_info->count >= MAX_MINMAX_COUNT)
        {
            if (adc > channel_info->max_values[channel_info->min_max_index])
            {
                channel_info->max_values[channel_info->min_max_index] = adc;
                _setup_lowest_max(channel_info);
            }
            else if (adc < channel_info->min_values[channel_info->max_min_index])
            {
                channel_info->min_values[channel_info->max_min_index] = adc;
                _setup_largest_min(channel_info);
            }
        }
        else
        {
            channel_info->max_values[channel_info->count] = adc;
            channel_info->min_values[channel_info->count] = adc;
        }
    }
    else
    {
        if (adc < channel_info->min_values[0])
            channel_info->min_values[0] = adc;
        else if (adc > channel_info->max_values[0])
            channel_info->max_values[0] = adc;
    }

    channel_info->total_value += adc;
    channel_info->count ++;

    if (channel_info->average_limits && channel_info->count == MAX_MINMAX_COUNT)
    {
        _setup_lowest_max(channel_info);
        _setup_largest_min(channel_info);
    }
}


void osm_adc_loop_interate(void)
{
    uint64_t now = osm_get_since_boot_ms();

    if ((now - _last_sample) < _collection_ms)
        return;

    osm_adc_debug("ADCS read buffers swap.");

    for(unsigned n = 0; n < ADCS_CHANNEL_COUNT; n++)
    {
        volatile adc_channel_info_t * channel_info = &_adc_channel_info[n];

        _adc_channel_info_cur[n] = *channel_info;

        if (!channel_info->average_limits)
        {
            channel_info->max_values[0] = 0;
            channel_info->min_values[0] = 0xFFFF;
        }

        channel_info->total_value  = 0;
        channel_info->count = 0;
    }

    _last_sample = now;
}


bool     adc_enable_averaged_limits(unsigned adc, bool enable)
{
    if (adc >= ADCS_CHANNEL_COUNT)
        return false;

    _adc_channel_info[adc].average_limits = enable;
    return true;
}


bool osm_adc_loop_set_interval(unsigned ms)
{
    if (ms < 50 || ms > 1000)
    {
        osm_adc_debug("Interval given too big or small.");
        return false;
    }
    osm_adc_debug("Set interval to %ums", ms);
    _collection_ms = ms;
    return true;
}


unsigned osm_adc_loop_get_interval(void)
{
    return _collection_ms;
}


static osm_command_response_t _cal_cb(char * args, osm_cmd_ctx_t * ctx)
{
    /* cal <adc name/num> <O/S/E/0..3>:<value>
     * e.g.
     * cal ADC_MON_5V
     * cal 5
     * cal ADC_MON_5V E:-0.42123
     * cal 5 2:-0.42123
     */
    char * p = NULL;
    unsigned adc_index;
    if (!osm_parse_to_index(args, &p, &adc_index, osm_model_get_adc_index))
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC.");
        goto bad_exit;
    }

    double* cal = osm_model_get_adc_cal(adc_index);
    if (!cal)
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC cal.");
        goto bad_exit;
    }
    const char* unit = osm_model_get_adc_unit(adc_index);
    if (!unit)
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC unit.");
        goto bad_exit;
    }

    const char * name = osm_model_get_adc_name(adc_index);

    char * np = NULL;
    unsigned coeff_num;
    switch (*p)
    {
        case 'O':
            /* Set offset */
            coeff_num = 0;
            p++;
            break;
        case 'S':
            /* Set scale */
            coeff_num = 1;
            p++;
            break;
        case 'E':
            /* Set exponential? */
            coeff_num = 2;
            p++;
            break;
        case 'U':
            p++;
            if (*p != ':')
            {
                osm_cmd_ctx_error(ctx, "Bad syntax");
                goto bad_exit;
            }
            p++;

            if (!osm_model_set_adc_unit(adc_index, p))
            {
                osm_cmd_ctx_error(ctx, "Failed to set ADC unit.");
                goto bad_exit;
            }
            osm_cmd_ctx_out(ctx, "Unit: %s", p);
            return OSM_COMMAND_RESP_OK;
        default:
            coeff_num = strtoul(p, &np, 10);
            if (p == np)
            {
                /* Print coeffs */
                osm_cmd_ctx_out(ctx, "ADC-CAL %u \"%s\"", adc_index, name);
                for (unsigned i = 0; i < MAX_ADC_CAL_POLY_NUM; i++)
                {
                    osm_cmd_ctx_out(ctx, "%u: %G", i, cal[i]);
                }
                osm_cmd_ctx_out(ctx, "Unit: %s", unit);
                return OSM_COMMAND_RESP_OK;
            }
            p = np;
            break;
    }
    if (*p != ':')
    {
        osm_cmd_ctx_error(ctx, "Bad syntax");
        goto bad_exit;
    }
    p++;

    if (coeff_num >= MAX_ADC_CAL_POLY_NUM)
    {
        osm_cmd_ctx_error(ctx, "Coefficient not supported.");
        goto bad_exit;
    }

    cal[coeff_num] = strtod(p, &np);

    if (p == np)
    {
        osm_cmd_ctx_error(ctx, "Failed to read calibration value.");
        goto bad_exit;
    }
    osm_cmd_ctx_out(ctx, "Set coeff %u for %s to %G", coeff_num, name, cal[coeff_num]);
    return OSM_COMMAND_RESP_OK;

bad_exit:
    return OSM_COMMAND_RESP_ERR;
}


static double _get_adc_called_value(double value, double* cal)
{
    double total = 0;

    for (unsigned i = 0; i < MAX_ADC_CAL_POLY_NUM; i++)
    {
        double coeff = cal[i];

        if (coeff == 0)
            continue;

        double curr = 1;

        for (unsigned j = 0; j < i; j++)
            curr *= value;

        curr *= coeff;

        total += curr;
    }
    return total;
}


static bool _adc_log_calc(unsigned adc, double value, const char * name, osm_cmd_ctx_t * ctx)
{
    if (!_use_calibration)
        return false;
    const char* unit = osm_model_get_adc_unit(adc);
    if (!unit)
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC unit.");
        return false;
    }
    double* cal = osm_model_get_adc_cal(adc);
    if (!cal)
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC cal.");
        return false;
    }

    double v = _get_adc_called_value(value, cal);

    osm_cmd_ctx_out(ctx, "%s : %G%s", name, v, unit);
    return true;
}


static osm_command_response_t _adc_log(unsigned adc, osm_cmd_ctx_t * ctx)
{
    const char * name = osm_model_get_adc_name(adc);

    volatile adc_channel_info_t * channel_info = &_adc_channel_info_cur[adc];

    osm_cmd_ctx_out(ctx, "ADC %u \"%s\"", adc, name);

    if (channel_info->average_limits)
    {
        unsigned min_value = 0;
        unsigned max_value = 0;

        for(unsigned n = 0; n < MAX_MINMAX_COUNT; n++)
            min_value += channel_info->min_values[n];

        for(unsigned n = 0; n < MAX_MINMAX_COUNT; n++)
            max_value += channel_info->max_values[n];

        if (!_adc_log_calc(adc, min_value / (double)MAX_MINMAX_COUNT, "Min", ctx))
            osm_cmd_ctx_out(ctx, "Min : %u / %u", min_value, MAX_MINMAX_COUNT);

        if (!_adc_log_calc(adc, max_value / (double)MAX_MINMAX_COUNT, "Max", ctx))
            osm_cmd_ctx_out(ctx, "Max : %u / %u", max_value, MAX_MINMAX_COUNT);

        if (!_adc_log_calc(adc, channel_info->total_value / (double) channel_info->count, "Avg", ctx))
            osm_cmd_ctx_out(ctx, "Avg : %u / %u", (unsigned)channel_info->total_value, (unsigned)channel_info->count);
    }
    else
    {
        if (!_adc_log_calc(adc, channel_info->min_values[0], "Min", ctx))
            osm_cmd_ctx_out(ctx, "Min : %u", channel_info->min_values[0]);

        if (!_adc_log_calc(adc, channel_info->max_values[0], "Max", ctx))
            osm_cmd_ctx_out(ctx, "Max : %u", channel_info->max_values[0]);

        if (!_adc_log_calc(adc, channel_info->total_value / (double)channel_info->count, "Avg", ctx))
            osm_cmd_ctx_out(ctx, "Avg : %u / %u", (unsigned)channel_info->total_value, (unsigned)channel_info->count);
    }

    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _adc_cb(char * args, osm_cmd_ctx_t * ctx)
{
    unsigned adc_index;
    if (!osm_parse_to_index(args, NULL, &adc_index, osm_model_get_adc_index))
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC.");
        goto bad_exit;
    }
    return _adc_log(adc_index, ctx);
bad_exit:
    return OSM_COMMAND_RESP_ERR;
}

static osm_command_response_t _adcs_cb(char * args, osm_cmd_ctx_t * ctx)
{
    for(unsigned n = 0; n < ADCS_CHANNEL_COUNT; n++)
        _adc_log(n, ctx);
    return OSM_COMMAND_RESP_ERR;
}

static bool _adcsw_done(void *userdata)
{
    return (*(uint64_t*)userdata) != _last_sample;
}

static osm_command_response_t _adcsw_cb(char * args, osm_cmd_ctx_t * ctx)
{
    uint64_t cur = _last_sample;

    uint64_t now = osm_get_since_boot_ms();

    uint64_t endtime = cur + _collection_ms;

    if (endtime > now)
    {
        uint64_t delta = endtime - now;
        osm_adc_debug("Waiting for %"PRIu64"ms", delta);
        osm_main_loop_iterate_for(delta, _adcsw_done, &cur);
    }

    for(unsigned n = 0; n < ADCS_CHANNEL_COUNT; n++)
        _adc_log(n, ctx);

    return OSM_COMMAND_RESP_OK;
}

static osm_command_response_t _adc_name_cb(char * args, osm_cmd_ctx_t * ctx)
{
    unsigned adc_index;
    char * p;
    if (!osm_parse_to_index(args, &p, &adc_index, osm_model_get_adc_index))
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC.");
        return OSM_COMMAND_RESP_ERR;
    }

    if (*p)
    {
        if (!osm_model_set_adc_name(adc_index, p))
        {
            osm_cmd_ctx_error(ctx, "Failed to set ADC name.");
            return OSM_COMMAND_RESP_ERR;
        }
        osm_cmd_ctx_out(ctx, "Renamed ADC %u to \"%s\"", adc_index, p);
    }
    else
    {
        const char * name = osm_model_get_adc_name(adc_index);
        osm_cmd_ctx_out(ctx, "ADC %u : \"%s\"", adc_index, name);
    }

    return OSM_COMMAND_RESP_OK;
}

static osm_command_response_t _adc_lavg_cb(char * args, osm_cmd_ctx_t * ctx)
{
    char * pos = NULL;
    unsigned adc_index;
    if (!osm_parse_to_index(args, &pos, &adc_index, osm_model_get_adc_index))
    {
        osm_cmd_ctx_error(ctx, "Failed to get ADC.");
        return OSM_COMMAND_RESP_ERR;
    }
    bool enable = (pos[0]=='1' || tolower(pos[0]) == 't')?true:false;

    _adc_channel_info[adc_index].average_limits = enable;

    if (enable)
        osm_cmd_ctx_out(ctx, "Averaged Limits Enabled for ADC %u", adc_index);
    else
        osm_cmd_ctx_out(ctx, "Averaged Limits Disabled for ADC %u", adc_index);

    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _adc_rate_cb(char * args, osm_cmd_ctx_t * ctx)
{
    if (*args)
    {
        unsigned ms = strtoul(args, NULL, 10);

        if (!osm_adc_loop_set_interval(ms))
        {
            osm_cmd_ctx_error(ctx, "Failed to set ADCs interval.");
            return OSM_COMMAND_RESP_ERR;
        }
    }

    osm_cmd_ctx_out(ctx, "ADC sampling interval : %ums", _collection_ms);
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _en_cal_cb(char * args, osm_cmd_ctx_t * ctx)
{
    if (*args)
    {
        if (strncmp(args, "ON", 2) == 0 || args[0] == '1')
        {
            _use_calibration = true;
            osm_cmd_ctx_out(ctx, "Enabled ADC Cals");
        }
        else
        {
            _use_calibration = false;
            osm_cmd_ctx_out(ctx, "Disabled ADC Cals");
        }
    }
    else
    {
        if (_use_calibration)
            osm_cmd_ctx_out(ctx, "ADC Cals Enabled");
        else
            osm_cmd_ctx_out(ctx, "ADC Cals Disabled");
    }
    return OSM_COMMAND_RESP_OK;
}


struct osm_cmd_link_t* osm_adc_loop_add(struct osm_cmd_link_t* tail)
{
    static struct osm_cmd_link_t cmds[] = {{ "cal",      "Get/Set ADC callibration.",    _cal_cb, false, NULL },
                                           { "adc",      "Print ADC.",                   _adc_cb, false, NULL },
                                           { "adcs",     "Print all ADCs.",              _adcs_cb, false, NULL },
                                           { "adcsw",    "Wait to print all ADCs.",      _adcsw_cb, false, NULL },
                                           { "adc_name", "Get/Set name of ADC.",         _adc_name_cb, false, NULL },
                                           { "adc_lavg", "Get/Set ADC limit averaging.", _adc_lavg_cb, false, NULL },
                                           { "adc_ms",   "Get/Set ADC rate in ms.",      _adc_rate_cb, false, NULL },
                                           { "en_cal",   "Enable ADC Calibration.",      _en_cal_cb, false, NULL},
    };
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
