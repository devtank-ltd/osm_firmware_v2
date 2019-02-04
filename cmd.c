#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "pinmap.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "usb_uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "inputs.h"
#include "outputs.h"
#include "timers.h"

static char   * rx_buffer;
static unsigned rx_buffer_len = 0;
static unsigned rx_pos        = 0;


typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(void);
} cmd_t;


static void uart_broadcast_cb();
static void debug_cb();
static void pps_cb();
static void adc_cb();
static void input_cb();
static void output_cb();
static void count_cb();
static void all_cb();
static void adc_sps_cb();
static void adc_start_cb();
static void adc_stop_cb();
static void adc_is_on_cb();
static void pps_start_cb();
static void pps_stop_cb();
static void pps_is_on_cb();


static cmd_t cmds[] = {
    { "uarts",    "Broad message to uarts.", uart_broadcast_cb},
    { "debug",    "Toggle debug",            debug_cb},
    { "ppss",     "Print all pulse info.",   pulsecount_log},
    { "pps",      "Print pulse info.",       pps_cb},
    { "adc",      "Print ADC.",              adc_cb},
    { "adcs",     "Print all ADCs.",         adcs_log},
    { "inputs",   "Print all inputs.",       inputs_log},
    { "outputs",  "Print all outputs.",      outputs_log},
    { "input",    "Print input.",            input_cb},
    { "output",   "Get/set output on/off.",  output_cb},
    { "count",    "Counts of controls.",     count_cb},
    { "all",      "Print everything.",       all_cb},
    { "adc_sps",  "Get/Set ADC SPS",         adc_sps_cb},
    { "adc_start","Start ADC sampling.",     adc_start_cb},
    { "adc_stop", "Stop ADC sampling.",      adc_stop_cb},
    { "adc_is_on","Is ADCs sampling.",       adc_is_on_cb},
    { "pps_start","Start PPS sampling.",     pps_start_cb},
    { "pps_stop", "Stop PPS sampling.",      pps_stop_cb},
    { "pps_is_on","Is PPS sampling.",        pps_is_on_cb},
    { NULL },
};


static bool cmd_has_arg()
{
    return (rx_buffer[rx_pos] == ' ');
}


void uart_broadcast_cb()
{
    uart_out(1, "High\n\r", 6);
    uart_out(2, "Low1\n\r", 6);
    uart_out(3, "Low2\n\r", 6);

    usb_uart_send(0, "High\n\r", 6);
    usb_uart_send(1, "Low1\n\r", 6);
    usb_uart_send(2, "Low2\n\r", 6);
}


void debug_cb()
{
    log_show_debug = !log_show_debug;
    log_out("Debug %s", (log_show_debug)?"ON":"OFF");
}


void pps_cb()
{
    unsigned pps = strtoul(rx_buffer + rx_pos, NULL, 10);

    pulsecount_pps_log(pps);
}


void adc_cb()
{
    unsigned adc = strtoul(rx_buffer + rx_pos, NULL, 10);
    adcs_adc_log(adc);
}


void input_cb()
{
    unsigned input = strtoul(rx_buffer + rx_pos, NULL, 10);
    inputs_input_log(input);
}


void output_cb()
{
    char * pos = NULL;
    unsigned output = strtoul(rx_buffer + rx_pos, &pos, 10);

    while(*pos == ' ')
        pos++;

    if (*pos)
    {
        bool on_off = (strtoul(pos, NULL, 10))?true:false;

        log_out("Set output %02u to %s", output, (on_off)?"ON":"OFF");
        outputs_set(output, on_off);
    }
    else output_output_log(output);
}


void count_cb()
{
    log_out("PPSS    : %u", pulsecount_get_count());
    log_out("Inputs  : %u", inputs_get_count());
    log_out("Outputs : %u", outputs_get_count());
    log_out("ADCs    : %u", adcs_get_count());
}


void all_cb()
{
    pulsecount_log();
    inputs_log();
    outputs_log();
    adcs_log();
}



void adc_sps_cb()
{
    if (cmd_has_arg())
    {
        unsigned sps = strtoul(rx_buffer + rx_pos, NULL, 10);
        timers_fast_set_rate(sps);
        log_out("Sample rate now : %u", sps);
    }
    else log_out("Sample rate : %u", timers_fast_get_rate());
}


void adc_start_cb()
{
    adcs_enable_sampling(true);
    log_out("ADC started with SPS %u", timers_fast_get_rate());
}


void adc_stop_cb()
{
    adcs_enable_sampling(false);
    log_out("ADC stopped.");
}


void adc_is_on_cb()
{
    if (adcs_is_enabled())
        log_out("ADCs sampling.");
    else
        log_out("ADCs not sampling.");
}


void pps_start_cb()
{
    pulsecount_start();
    log_out("PPS started");
}


void pps_stop_cb()
{
    pulsecount_start();
    log_out("PPS stopped");
}


void pps_is_on_cb()
{
    if (pulsecount_is_running())
        log_out("PPS sampling.");
    else
        log_out("PPS not sampling.");
}


void cmds_process(char * command, unsigned len)
{
    if (!len)
        return;

    rx_buffer = command;
    rx_buffer_len = len;

    bool found = false;
    log_out(LOG_START_SPACER);
    for(cmd_t * cmd = cmds; cmd->key; cmd++)
    {
        unsigned keylen = strlen(cmd->key);
        if(rx_buffer_len >= keylen &&
           !strncmp(cmd->key, rx_buffer, keylen) &&
           (rx_buffer[keylen] == '\0' || rx_buffer[keylen] == ' '))
        {
            rx_pos = keylen;
            found = true;
            cmd->cb();
            break;
        }
    }
    if (!found)
    {
        log_out("Unknown command \"%s\"", rx_buffer);
        log_out(LOG_SPACER);
        for(cmd_t * cmd = cmds; cmd->key; cmd++)
            log_out("%10s : %s", cmd->key, cmd->desc);
    }
    log_out(LOG_END_SPACER);
}



void cmds_init()
{
}
