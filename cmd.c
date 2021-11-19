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
#include "modbus.h"

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
static void version_cb(char *args);
static void audio_dump_cb(char *args);
static void lora_cb(char *args);
static void lora_config_cb(char *args);
static void interval_cb(char *args);
static void debug_cb(char *args);
static void hmp_cb(char *args);
static void modbus_setup_cb(char *args);
static void modbus_test_cb(char *args);


static cmd_t cmds[] = {
    { "ios",      "Print all IOs.",          ios_log},
    { "io",       "Get/set IO set.",         io_cb},
    { "sio",      "Enable Special IO.",      special_cb},
    { "count",    "Counts of controls.",     count_cb},
    { "version",  "Print version.",          version_cb},
    { "audio_dump", "Do audiodump",          audio_dump_cb},
    { "lora",      "Send lora message",      lora_cb},
    { "lora_config", "Set lora config",      lora_config_cb},
    { "interval", "Set the interval",        interval_cb},
    { "debug",     "Set hex debug mask",     debug_cb},
    { "hpm",       "Enable/Disable HPM",     hmp_cb},
    { "mb_setup",  "Change Modbus comms",    modbus_setup_cb},
    { "mb_test",    "Read modbus reg",       modbus_test_cb},
    { NULL },
};


char * skip_space(char * pos)
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


void lora_config_cb(char * args)
{
    // CMD  : "lora_config dev-eui 118f875d6994bbfd"
    // ARGS : "dev-eui 118f875d6994bbfd"
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p == NULL)
    {
        return;
    }
    uint8_t end_pos_word = p - args + 1;
    p = skip_space(p);
    if (strncmp(args, "dev-eui", end_pos_word-1) == 0)
    {
        char eui[LW__DEV_EUI_LEN] = "";
        strncpy(eui, p, strlen(p));
        persist_set_lw_dev_eui(eui);
    }
    else if (strncmp(args, "app-key", end_pos_word-1) == 0)
    {
        char key[LW__APP_KEY_LEN] = "";
        strncpy(key, p, strlen(p));
        persist_set_lw_dev_eui(key);
    }
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
    uint8_t uuid;
    if (!measurements_get_uuid(name, &uuid))
    {
        return;
    }
    uint16_t interval;
    p = skip_space(p);
    if (p[0] == '?')
    {
        if (persist_get_interval(uuid, &interval))
        {
            log_out("interval = %"PRIu8" x 5mins", interval);
        }
        else
        {
            log_out("No record.");
        }
    }
    else if (isdigit((unsigned char)*p))
    {
        interval = strtoul(p, NULL, 10);
        measurements_set_interval_uuid(uuid, interval);
        persist_set_interval(uuid, interval);
        log_out("Set interval for %s to %u x 5mins", name, interval);
    }
}


static void debug_cb(char * args)
{
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    unsigned mask = strtoul(pos, &pos, 16);

    mask |= DEBUG_SYS;

    uint32_t prev_mask = persist_get_log_debug_mask();

    log_debug(DEBUG_SYS, "Setting debug mask to 0x%x", mask);
    log_out("Previous mask was 0x%"PRIx32, prev_mask);
    log_debug_mask = mask;
    persist_set_log_debug_mask(mask);
}


static void hmp_cb(char *args)
{
    if (args[0] != '0')
    {
        hmp_enable(true);

        uint16_t pm25;
        uint16_t pm10;

        if (hmp_get(&pm25, &pm10))
        {
            log_out("PM25:%"PRIu16", PM10:%"PRIu16, pm25, pm10);
            measurements_write_data_value(MEASUREMENT_UUID__PM10, (value_t)pm10);
            measurements_write_data_value(MEASUREMENT_UUID__PM25, (value_t)pm25);
            measurements_send(1);
        }
        else
        {
            log_out("No HPM data.");
        }
    }
    else
    {
        hmp_enable(false);
        log_out("HPM disabled");
    }
}


static void modbus_setup_cb(char *args)
{
    /*<BIN/RTU> <SPEED> <BITS><PARITY><STOP>
     * EXAMPLE: RTU 115200 8N1
     */
    modbus_setup_from_str(args);
}


static void modbus_cmd_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    for(unsigned n = 0; n < size; n++)
        log_debug(DEBUG_MODBUS, "0x%"PRIx8, data[n]);
}


static void modbus_test_cb(char *args)
{
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    static modbus_reg_t cmd_reg = {.name = "CMD", .func = MODBUS_READ_HOLDING_FUNC};

    cmd_reg.slave_id = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    cmd_reg.addr = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    cmd_reg.len = strtoul(pos, NULL, 10);

    log_debug_mask |= DEBUG_MODBUS;

    if (modbus_start_read(&cmd_reg, modbus_cmd_cb))
        log_out("Modbus read sent");
    else
        log_out("Modbus read not sent");
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
