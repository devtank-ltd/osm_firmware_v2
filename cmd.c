#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <libopencm3/stm32/gpio.h>

#include "pinmap.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "io.h"
#include "timers.h"
#include "persist_config.h"
#include "sai.h"
#include "lorawan.h"
#include "measurements.h"
#include "hpm.h"

static char   * rx_buffer;
static unsigned rx_buffer_len = 0;


typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(char * args);
} cmd_t;


static void io_cb(char *args);
static void special_cb(char *args);
static void count_cb(char *args);
static void persist_cb(char *args);
static void en_cal_cb(char *args);
static void cal_name_cb(char *args);
static void version_cb(char *args);
static void audio_dump_cb(char *args);
static void lora_cb(char *args);
static void interval_cb(char *args);
static void debug_cb(char *args);
static void hmp_cb(char *args);


static cmd_t cmds[] = {
    { "ios",      "Print all IOs.",          ios_log},
    { "io",       "Get/set IO set.",         io_cb},
    { "sio",      "Enable Special IO.",      special_cb},
    { "count",    "Counts of controls.",     count_cb},
    { "persist",  "Start persist of name.",  persist_cb},
    { "en_cal",   "Enable ADC Calibration.", en_cal_cb},
    { "cal_name", "Get Calibration Name",    cal_name_cb},
    { "version",  "Print version.",          version_cb},
    { "audio_dump", "Do audiodump",          audio_dump_cb},
    { "lora",      "Send lora message",      lora_cb},
    { "interval", "Set the interval",        interval_cb},
    { "debug",     "Set hex debug mask",     debug_cb},
    { "hpm",       "Enable/Disable HPM",     hmp_cb},
    { NULL },
};


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


void io_cb(char *args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);
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


void special_cb(char * args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);

    if (io_enable_special(io))
        log_out("IO %02u special enabled", io);
    else
        log_out("IO %02u has no special", io);
}


void count_cb(char * args)
{
    log_out("IOs     : %u", ios_get_count());
    log_out("UARTs   : %u", UART_CHANNELS_COUNT-1); /* Control/Debug is left */
}


void uart_cb(char * args)
{
    char * pos = NULL;
    unsigned uart = strtoul(args, &pos, 10);

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
            if (isdigit((unsigned char)*pos))
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


void persist_cb(char * args)
{
    char * pos = skip_space(args);

    if (*pos)
        persistent_set_name(pos);

    log_out("New persistent data started.");
}


void en_cal_cb(char * args)
{
    char * pos = skip_space(args);

    if (*pos)
    {
        if (strncmp(pos, "ON", 2) == 0 || pos[0] == '1')
        {
            if (persistent_set_use_cal(true))
                log_out("Enabled ADC Cals");
            else
                log_debug(DEBUG_SYS, "Failed to enable ADC Cals");
        }
        else
        {
            log_out("Disabled ADC Cals");
            if (!persistent_set_use_cal(false))
                log_debug(DEBUG_SYS, "Failed to disable ADC Cals");
        }
    }
    else
    {
        if (persistent_get_use_cal())
            log_out("ADC Cals Enabled");
        else
            log_out("ADC Cals Disabled");
    }
}


void cal_name_cb(char * args)
{
    const char * name = persistent_get_name();
    log_out("N:%s", name);
}


void version_cb(char * args)
{
    log_out("Version : %s", GIT_VERSION);
}


void audio_dump_cb(char * args)
{
    start_audio_dumping = true;
}


void lora_cb(char * args)
{
    char * pos = skip_space(args);
    lw_send(pos);
}


void interval_cb(char * args)
{
    // CMD  : "interval temperature 3"
    // ARGS : "temperature 3"

    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p == NULL)
    {
        return;
    }
    uint8_t end_pos_word = p - args + 1;
    char name[32] = {0};
    memset(name, 0, end_pos_word);
    strncpy(name, args, end_pos_word-1);
    p = skip_space(p);
    uint8_t new_interval = strtoul(p, NULL, 10);

    measurements_set_interval(name, new_interval);
}


static void debug_cb(char * args)
{
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    unsigned mask = strtoul(pos, &pos, 16);

    mask |= DEBUG_SYS;

    log_debug(DEBUG_SYS, "Setting debug mask to 0x%x", mask);
    log_debug_mask = mask;
}


static void hmp_cb(char *args)
{
    if (args[0] != '0')
    {
        hmp_enable(true);

        uint16_t pm25;
        uint16_t pm10;

        if (hmp_get(&pm25, &pm10))
            log_out("PM25:%"PRIu16", PM10:%"PRIu16, pm25, pm10);
        else
            log_out("No HPM data.");
    }
    else
    {
        hmp_enable(false);
        log_out("HPM disabled");
    }
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
    char * args;
    for(cmd_t * cmd = cmds; cmd->key; cmd++)
    {
        unsigned keylen = strlen(cmd->key);
        if(rx_buffer_len >= keylen &&
           !strncmp(cmd->key, rx_buffer, keylen) &&
           (rx_buffer[keylen] == '\0' || rx_buffer[keylen] == ' '))
        {
            found = true;
            args = skip_space(rx_buffer + keylen);
            cmd->cb(args);
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
