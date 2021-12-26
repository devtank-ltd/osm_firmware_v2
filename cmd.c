#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>

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
#include "modbus_measurements.h"
#include "update.h"

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
static void samplecount_cb(char * args);
static void debug_cb(char *args);
static void hpm_cb(char *args);
static void modbus_setup_cb(char *args);
static void modbus_add_dev_cb(char *args);
static void modbus_add_reg_cb(char *args);
static void modbus_get_reg_cb(char *args);
static void measurements_cb(char *args);
static void fw_add(char *args);
static void fw_fin(char *args);
static void reset_cb(char *args);


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
    { "samplecount", "Set the samplecount",  samplecount_cb},
    { "debug",     "Set hex debug mask",     debug_cb},
    { "hpm",       "Enable/Disable HPM",     hpm_cb},
    { "mb_setup",  "Change Modbus comms",    modbus_setup_cb},
    { "mb_dev_add","Add modbus dev",         modbus_add_dev_cb},
    { "mb_reg_add","Add modbus reg",         modbus_add_reg_cb},
    { "mb_get_reg","Get modbus reg",         modbus_get_reg_cb},
    { "mb_reg_del","Delete modbus reg",      modbus_measurement_del_reg},
    { "mb_dev_del","Delete modbus dev",      modbus_measurement_del_dev},
    { "mb_log",    "Show modbus setup",      modbus_log},
    { "mb_save",   "Save modbus setup",      modbus_save},
    { "measurements", "Print measurements",  measurements_cb},
    { "fw+",       "Add chunk of new fw.",   fw_add},
    { "fw@",       "Finishing crc of new fw.", fw_fin},
    { "reset",     "Reset device.",          reset_cb},
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
    lw_send_str(pos);
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
        char eui[LW__DEV_EUI_LEN + 1] = "";
        strncpy(eui, p, strlen(p));
        persist_set_lw_dev_eui(eui);
    }
    else if (strncmp(args, "app-key", end_pos_word-1) == 0)
    {
        char key[LW__APP_KEY_LEN + 1] = "";
        strncpy(key, p, strlen(p));
        persist_set_lw_app_key(key);
    }
}


static void interval_cb(char * args)
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
    char name[8] = {0};
    memset(name, 0, end_pos_word);
    strncpy(name, args, end_pos_word-1);
    p = skip_space(p);
    uint8_t new_interval = strtoul(p, NULL, 10);

    measurements_set_interval(name, new_interval);
    measurements_save();
}


static void samplecount_cb(char * args)
{
    // CMD  : "samplecount temperature 5"
    // ARGS : "temperature 5"

    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p == NULL)
    {
        return;
    }
    uint8_t end_pos_word = p - args + 1;
    char name[8] = {0};
    memset(name, 0, end_pos_word);
    strncpy(name, args, end_pos_word-1);
    p = skip_space(p);
    uint8_t new_samplecount = strtoul(p, NULL, 10);

    measurements_set_samplecount(name, new_samplecount);
    measurements_save();
}


static void debug_cb(char * args)
{
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    unsigned mask = strtoul(pos, &pos, 16);

    mask |= DEBUG_SYS;

    uint32_t prev_mask = persist_get_log_debug_mask();

    log_debug_mask = mask;
    persist_set_log_debug_mask(mask);
    log_debug(DEBUG_SYS, "Setting debug mask to 0x%x", mask);
    log_debug(DEBUG_SYS, "Previous mask was 0x%"PRIx32, prev_mask);
}


static void hpm_cb(char *args)
{
    if (args[0] != '0')
    {
        hpm_enable(true);

        uint16_t pm25;
        uint16_t pm10;

        if (hpm_get(&pm25, &pm10))
        {
            log_out("PM25:%"PRIu16", PM10:%"PRIu16, pm25, pm10);
        }
        else
        {
            log_out("No HPM data.");
        }
    }
    else
    {
        hpm_enable(false);
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


static void modbus_add_dev_cb(char * args)
{
    /*<slave_id> <name>
     * (name can only be 4 char long)
     * EXAMPLES:
     * 0x1 TEST
     */
    char * pos = skip_space(args);

    uint16_t slave_id = strtoul(pos, &pos, 16);

    char * name = skip_space(pos);

    if (modbus_add_device(slave_id, name))
        log_out("Added modbus device");
    else
        log_out("Failed to add modbus device.");
}


static void modbus_add_reg_cb(char * args)
{
    /*<slave_id> <reg_addr> <modbus_func> <type> <name>
     * (name can only be 4 char long)
     * Only Modbus Function 3, Hold Read supported right now.
     * 0x1 0x16 3 F   T-Hz
     * 1 22 3 F       T-Hz
     * 0x2 0x30 3 U16 T-As
     * 0x2 0x32 3 U32 T-Vs
     */
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    uint16_t slave_id = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    uint16_t reg_addr = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    uint8_t func = strtoul(pos, &pos, 10);

    pos = skip_space(pos);

    modbus_reg_type_t type = MODBUS_REG_TYPE_INVALID;

    if (pos[0] == 'U')
    {
        if (pos[1] == '1' && pos[2] == '6' && pos[3] == ' ')
        {
            type = MODBUS_REG_TYPE_U16;
        }
        else if (pos[1] == '3' && pos[2] == '2' && pos[3] == ' ')
        {
            type = MODBUS_REG_TYPE_U32;
        }
        else
        {
            log_out("Unknown modbus reg type.");
            return;
        }
        pos = skip_space(pos + 3);
    }
    else if (pos[0] == 'F' && pos[1] == ' ')
    {
        type = MODBUS_REG_TYPE_FLOAT;
        pos = skip_space(pos + 1);
    }
    else
    {
        log_out("Unknown modbus reg type.");
        return;
    }

    char * name = pos;

    modbus_dev_t * dev = modbus_get_device_by_id(slave_id);
    if (!dev)
    {
        log_out("Unknown modbus device.");
        return;
    }

    if (modbus_dev_add_reg(dev, name, type, func, reg_addr))
    {
        log_out("Added modbus reg");
        if (!modbus_measurement_add(modbus_dev_get_reg_by_name(dev, name)))
            log_out("Failed to add modbus reg to measurements!");
    }
    else log_out("Failed to add modbus reg.");
}


static void modbus_get_reg_cb(char * args)
{
    char * name = skip_space(args);

    modbus_reg_t * reg = modbus_get_reg(name);

    if (!reg)
    {
        log_out("Unknown modbus register.");
        return;
    }

    log_debug_mask |= DEBUG_MODBUS;

    modbus_start_read(reg);
}


static void measurements_cb(char *args)
{
    measurements_print();
    measurements_print_persist();
}


static void fw_add(char *args)
{
    args = skip_space(args);
    unsigned len = strlen(args);
    if (len%2)
    {
        log_error("Invalid fw chunk.");
	return;
    }
    char * end = args + len;
    while(args < end)
    {
        char * next = args + 2;
        char t = *next;
	*next=0;
	uint8_t d = strtoul(args, NULL, 16);
	*next=t;
	args=next;
        if (!fw_ota_add_chunk(&d, 1))
        {
            log_error("Invalid fw.");
	    return;
	}
    }
    log_out("FW chunk added");
}


static void fw_fin(char *args)
{
    args = skip_space(args);
    uint16_t crc = strtoul(args, NULL, 16);
    if (fw_ota_complete(crc))
        log_out("FW added");
    else
        log_error("FW adding failed.");
}


static void reset_cb(char *args)
{
    scb_reset_system();
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
