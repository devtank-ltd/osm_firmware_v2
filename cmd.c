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
#include "gpio.h"
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


static void pps_cb();
static void adc_cb();
static void input_cb();
static void output_cb();
static void gpio_cb();
static void count_cb();
static void uart_cb();
static void uarts_cb();
static void version_cb();


static cmd_t cmds[] = {
    { "ppss",     "Print all pulse info.",   pulsecount_log},
    { "pps",      "Print pulse info.",       pps_cb},
    { "adc",      "Print ADC.",              adc_cb},
    { "adcs",     "Print all ADCs.",         adcs_log},
    { "inputs",   "Print all inputs.",       inputs_log},
    { "outputs",  "Print all outputs.",      outputs_log},
    { "gpios",    "Print all gpios.",        gpios_log},
    { "input",    "Print input.",            input_cb},
    { "output",   "Get/set output on/off.",  output_cb},
    { "gpio",     "Get/set GPIO set.",       gpio_cb},
    { "count",    "Counts of controls.",     count_cb},
    { "uart",     "Change UART speed.",      uart_cb},
    { "uarts",    "Show UART speed.",        uarts_cb},
    { "version",  "Print version.",          version_cb},
    { NULL },
};




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


static char * skip_space(char * pos)
{
    while(*pos == ' ')
        pos++;
    return pos;
}

static char * skip_to_space(char * pos)
{
    while(*pos && *pos != ' ')
        pos++;
    return pos;
}



void output_cb()
{
    char * pos = NULL;
    unsigned output = strtoul(rx_buffer + rx_pos, &pos, 10);

    pos = skip_space(pos);
    if (*pos)
    {
        bool on_off = (strtoul(pos, NULL, 10))?true:false;

        log_out("Set output %02u to %s", output, (on_off)?"ON":"OFF");
        outputs_set(output, on_off);
    }
    else output_output_log(output);
}


void gpio_cb()
{
    char * pos = NULL;
    unsigned gpio = strtoul(rx_buffer + rx_pos, &pos, 10);
    pos = skip_space(pos);
    bool do_read = true;

    if (*pos == ':')
    {
        bool as_input;
        pos = skip_space(pos + 1);
        if (strncmp(pos, "IN", 2) == 0 || *pos == 'I')
        {
            pos = skip_to_space(pos);
            as_input = true;
        }
        else if (strncmp(pos, "OUT", 3) == 0 || *pos == 'O')
        {
            pos = skip_to_space(pos);
            as_input = false;
        }
        else
        {
            log_error("Malformed gpio type command");
            return;
        }

        int pull;
        pos = skip_space(pos);
        if (strncmp(pos, "UP", 2) == 0 || *pos == 'U')
        {
            pos = skip_to_space(pos);
            pull = 1;
        }
        else if (strncmp(pos, "DOWN", 4) == 0 || *pos == 'D')
        {
            pos = skip_to_space(pos);
            pull = -1;
        }
        else if (strncmp(pos, "NONE", 4) == 0 || *pos == 'N')
        {
            pos = skip_to_space(pos);
            pull = 0;
        }
        else
        {
            log_error("Malformed gpio pull command");
            return;
        }

        gpio_configure(gpio, as_input, pull);
        pos = skip_space(pos);
    }

    if (*pos == '=')
    {
        pos = skip_space(pos + 1);
        log_error("POS : %s", pos);
        if (strncmp(pos, "ON", 2) == 0 || *pos == '1')
        {
            pos = skip_to_space(pos);
            if (!gpio_is_input(gpio))
            {
                gpio_on(gpio, true);
                log_out("GPIO %02u = ON", gpio);
                do_read = false;
            }
            else log_error("GPIO %02u is input but output command.", gpio);
        }
        else if (strncmp(pos, "OFF", 3) == 0 || *pos == '0')
        {
            pos = skip_to_space(pos);
            if (!gpio_is_input(gpio))
            {
                gpio_on(gpio, false);
                log_out("GPIO %02u = OFF", gpio);
                do_read = false;
            }
            else log_error("GPIO %02u is input but output command.", gpio);
        }
        else
        {
            log_error("Malformed gpio on/off command");
            return;
        }
    }

    if (do_read)
        gpio_log(gpio);
}


void count_cb()
{
    log_out("PPSS    : %u", pulsecount_get_count());
    log_out("Inputs  : %u", inputs_get_count());
    log_out("Outputs : %u", outputs_get_count());
    log_out("ADCs    : %u", adcs_get_count());
    log_out("GPIOs   : %u", gpios_get_count());
    log_out("UARTs   : %u", UART_CHANNELS_COUNT-1); /* Control/Debug is left */
}


void uart_cb()
{
    char * pos = NULL;
    unsigned uart = strtoul(rx_buffer + rx_pos, &pos, 10);

    pos = skip_space(pos);
    if (*pos)
    {
        unsigned baud = strtoul(pos, NULL, 10);
        uart_set_baudrate(uart + 1, baud);
    }

    log_out("UART %u : %u", uart, uart_get_baudrate(uart + 1));
}


void uarts_cb()
{
    for(unsigned n = 0; n < (UART_CHANNELS_COUNT - 1); n++)
        log_out("UART %u : %u", n, uart_get_baudrate(n + 1));
}


void version_cb()
{
    log_out("Version : %s", GIT_VERSION);
}


void cmds_process(char * command, unsigned len)
{
    if (!len)
        return;

    log_debug(DEBUG_SYS, "Command \"%s\"", command);

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
