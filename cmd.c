#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libopencm3/stm32/gpio.h>

#include "pinmap.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "usb_uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "io.h"
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
static void io_cb();
static void special_cb();
static void count_cb();
static void uart_cb();
static void uarts_cb();
static void version_cb();


static cmd_t cmds[] = {
    { "ppss",     "Print all pulse info.",   pulsecount_log},
    { "pps",      "Print pulse info.",       pps_cb},
    { "adc",      "Print ADC.",              adc_cb},
    { "adcs",     "Print all ADCs.",         adcs_log},
    { "ios",      "Print all IOs.",          ios_log},
    { "io",       "Get/set IO set.",         io_cb},
    { "sio",      "Enable Special IO.",      special_cb},
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


void io_cb()
{
    char * pos = NULL;
    unsigned io = strtoul(rx_buffer + rx_pos, &pos, 10);
    pos = skip_space(pos);
    bool do_read = false;

    if (*pos == ':')
    {
        do_read = true;
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

        unsigned pull = GPIO_PUPD_NONE;

        pos = skip_space(pos);

        if (*pos && *pos != '=')
        {
            if (strncmp(pos, "UP", 2) == 0 || *pos == 'U')
            {
                pos = skip_to_space(pos);
                pull = GPIO_PUPD_PULLUP;
            }
            else if (strncmp(pos, "DOWN", 4) == 0 || *pos == 'D')
            {
                pos = skip_to_space(pos);
                pull = GPIO_PUPD_PULLDOWN;
            }
            else if (strncmp(pos, "NONE", 4) == 0 || *pos == 'N')
            {
                pos = skip_to_space(pos);
            }
            else
            {
                log_error("Malformed gpio pull command");
                return;
            }
            pos = skip_space(pos);
        }

        io_configure(io, as_input, pull);
    }

    if (*pos == '=')
    {
        pos = skip_space(pos + 1);
        if (strncmp(pos, "ON", 2) == 0 || *pos == '1')
        {
            pos = skip_to_space(pos);
            if (!io_is_input(io))
            {
                io_on(io, true);
                if (!do_read)
                    log_out("IO %02u = ON", io);
            }
            else log_error("IO %02u is input but output command.", io);
        }
        else if (strncmp(pos, "OFF", 3) == 0 || *pos == '0')
        {
            pos = skip_to_space(pos);
            if (!io_is_input(io))
            {
                io_on(io, false);
                if (!do_read)
                    log_out("IO %02u = OFF", io);
            }
            else log_error("IO %02u is input but output command.", io);
        }
        else
        {
            log_error("Malformed gpio on/off command");
            return;
        }
    }
    else do_read = true;

    if (do_read)
        io_log(io);
}


void special_cb()
{
    char * pos = NULL;
    unsigned io = strtoul(rx_buffer + rx_pos, &pos, 10);

    if (io_enable_special(io))
        log_out("IO %02u special enabled", io);
    else
        log_out("IO %02u has no special", io);
}


void count_cb()
{
    log_out("PPSS    : %u", pulsecount_get_count());
    log_out("ADCs    : %u", adcs_get_count());
    log_out("IOs     : %u", ios_get_count());
    log_out("UARTs   : %u", UART_CHANNELS_COUNT-1); /* Control/Debug is left */
}


void uart_cb()
{
    char * pos = NULL;
    unsigned uart = strtoul(rx_buffer + rx_pos, &pos, 10);

    unsigned         speed;
    uint8_t          databits;
    uart_parity_t    parity;
    uart_stop_bits_t stop;

    uart++;

    if (!uart_get_setup(uart, &speed, &databits, &parity, &stop))
    {
        log_error("INVALID UART GIVEN");
        return;
    }

    pos = skip_space(pos);
    if (*pos)
    {
        speed = strtoul(pos, NULL, 10);
        pos = skip_space(++pos);
        if (*pos)
        {
            if (isdigit(*pos))
            {
                databits = (uint8_t)(*pos) - (uint8_t)'0';
                pos = skip_space(++pos);
            }

            switch(*pos)
            {
                case 'N' : parity = uart_parity_none; break;
                case 'E' : parity = uart_parity_even; break;
                case 'O' : parity = uart_parity_odd; break;
                default: break;
            }
            pos = skip_space(++pos);

            switch(*pos)
            {
                case '1' : stop = uart_stop_bits_1; break;
                case '2' : stop = uart_stop_bits_2; break;
                default: break;
            }
        }

        uart_resetup(uart, speed, databits, parity, stop);
    }

    log_out("UART %u : %u %"PRIu8"%c%"PRIu8, uart - 1,
        speed, databits, uart_parity_as_char(parity), uart_stop_bits_as_int(stop));
}


void uarts_cb()
{
    for(unsigned n = 0; n < (UART_CHANNELS_COUNT - 1); n++)
    {
        unsigned         speed;
        uint8_t          databits;
        uart_parity_t    parity;
        uart_stop_bits_t stop;

        if (!uart_get_setup(n + 1, &speed, &databits, &parity, &stop))
        {
            log_error("INVALID UART GIVEN");
            return;
        }

        log_out("UART %u : %u %"PRIu8"%c%"PRIu8, n,
            speed, databits, uart_parity_as_char(parity), uart_stop_bits_as_int(stop));
    }
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
