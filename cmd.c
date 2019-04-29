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


static void debug_cb();
static void pps_cb();
static void adc_cb();
static void input_cb();
static void output_cb();
static void count_cb();
static void all_cb();


static cmd_t cmds[] = {
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
    { NULL },
};




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


void cmds_process(char * command, unsigned len)
{
    if (!len)
        return;

    log_debug("Command \"%s\"", command);

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
