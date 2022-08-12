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
#include "ds18b20.h"
#include "common.h"
#include "htu21d.h"
#include "log.h"
#include "uart_rings.h"
#include "veml7700.h"
#include "cc.h"
#include "bat.h"
#include "sleep.h"
#include "can_impl.h"
#include "debug_mode.h"


static char   * rx_buffer;
static unsigned rx_buffer_len = 0;


typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(char * args);
} cmd_t;




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


void cmd_enable_pulsecount_cb(char * args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);
    if (args == pos)
        goto bad_exit;
    pos = skip_space(pos);
    uint8_t pupd;
    if (pos[0] == 'U')
        pupd = GPIO_PUPD_PULLUP;
    else if (pos[0] == 'D')
        pupd = GPIO_PUPD_PULLDOWN;
    else if (pos[0] == 'N')
        pupd = GPIO_PUPD_NONE;
    else
        goto bad_exit;

    if (io_enable_pulsecount(io, pupd))
        log_out("IO %02u pulsecount enabled", io);
    else
        log_out("IO %02u has no pulsecount", io);
    return;
bad_exit:
    log_out("<io> <U/D/N>");
}


void cmd_enable_onewire_cb(char * args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);

    if (io_enable_w1(io))
        log_out("IO %02u onewire enabled", io);
    else
        log_out("IO %02u has no onewire", io);
}


void count_cb(char * args)
{
    log_out("IOs     : %u", ios_get_count());
}


void version_cb(char * args)
{
    log_out("Version : %s", GIT_VERSION);
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

    uint16_t lenrem = strnlen(p, CMD_LINELEN);
    if (lenrem == 0)
        goto syntax_exit;

    char* np = strchr(p, ' ');
    if (!np)
        np = p + lenrem;
    uint8_t wordlen = np - p;

    if (strncmp(p, "dev-eui", wordlen) == 0)
    {
        /* Dev EUI */
        char eui[LW_DEV_EUI_LEN + 1];
        p = skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Dev EUI */
            if (!persist_get_lw_dev_eui(eui))
            {
                log_out("Could not get Dev EUI");
                return;
            }
            log_out("Dev EUI: %s", eui);
            return;
        }
        /* Set Dev EUI */
        if (lenrem != LW_DEV_EUI_LEN)
        {
            log_out("Dev EUI should be %"PRIu16" characters long. (%"PRIu8")", LW_DEV_EUI_LEN, lenrem);
            return;
        }
        strncpy(eui, p, lenrem);
        eui[lenrem] = 0;
        persist_set_lw_dev_eui(eui);
        lw_reload_config();
        return;
    }
    if (strncmp(p, "app-key", wordlen) == 0)
    {
        /* App Key */
        char key[LW_APP_KEY_LEN + 1];
        p = skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Dev EUI */
            if (!persist_get_lw_app_key(key))
            {
                log_out("Could not get app key.");
                return;
            }
            log_out("App Key: %s", key);
            return;
        }
        /* Set Dev EUI */
        if (lenrem != LW_APP_KEY_LEN)
        {
            log_out("App key should be %"PRIu16" characters long. (%"PRIu8")", LW_APP_KEY_LEN, lenrem);
            return;
        }
        strncpy(key, p, lenrem);
        key[lenrem] = 0;
        persist_set_lw_app_key(key);
        lw_reload_config();
        return;
    }
syntax_exit:
    log_out("lora_config dev-eui/app-key [EUI/KEY]");
}


static void interval_cb(char * args)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit(p[0]))
    {
        uint8_t new_interval = strtoul(p, NULL, 10);

        if (measurements_set_interval(name, new_interval))
        {
            log_out("Changed %s interval to %"PRIu8, name, new_interval);
        }
        else log_out("Unknown measuremnt");
    }
    else
    {
        uint8_t interval;
        if (measurements_get_interval(name, &interval))
        {
            log_out("Interval of %s = %"PRIu8, name, interval);
        }
        else
        {
            log_out("Unknown measuremnt");
        }
    }
}


static void samplecount_cb(char * args)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit(p[0]))
    {
        uint8_t new_samplecount = strtoul(p, NULL, 10);

        if (measurements_set_samplecount(name, new_samplecount))
        {
            log_out("Changed %s samplecount to %"PRIu8, name, new_samplecount);
        }
        else log_out("Unknown measuremnt");
    }
    else
    {
        uint8_t samplecount;
        if (measurements_get_samplecount(name, &samplecount))
        {
            log_out("Samplecount of %s = %"PRIu8, name, samplecount);
        }
        else
        {
            log_out("Unknown measuremnt");
        }
    }
}


static void debug_cb(char * args)
{
    char * pos = skip_space(args);

    log_out("Debug mask : 0x%"PRIx32, log_debug_mask);

    if (pos[0])
    {
        if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
            pos += 2;

        unsigned mask = strtoul(pos, &pos, 16);

        mask |= DEBUG_SYS;

        log_debug_mask = mask;
        persist_set_log_debug_mask(mask);
        log_out("Setting debug mask to 0x%x", mask);
    }
}


static void hpm_cb(char *args)
{
    if (args[0] == '1')
    {
        hpm_enable(true);
        log_out("HPM enabled");
    }

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

    if (args[0] == '0')
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
    /*<slave_id> <LSB/MSB> <LSW/MSW> <name>
     * (name can only be 4 char long)
     * EXAMPLES:
     * 0x1 MSB MSW TEST
     */
    if (!modbus_add_dev_from_str(args))
    {
        log_out("<slave_id> <LSB/MSB> <LSW/MSW> <name>");
    }
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

    modbus_reg_type_t type = modbus_reg_type_from_str(pos, (const char**)&pos);
    if (type == MODBUS_REG_TYPE_INVALID)
        return;

    pos = skip_space(pos);

    char * name = pos;

    modbus_dev_t * dev = modbus_get_device_by_id(slave_id);
    if (!dev)
    {
        log_out("Unknown modbus device.");
        return;
    }

    if (modbus_dev_add_reg(dev, name, type, func, reg_addr))
    {
        log_out("Added modbus reg %s", name);
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
    log_out("FW %u chunk added", len/2);
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


static void cc_cb(char* args)
{
    char* p;
    uint8_t cc_num = strtoul(args, &p, 10);
    value_t value_1;
    if (p == args)
    {
        value_t value_2, value_3;
        if (!cc_get_all_blocking(&value_1, &value_2, &value_3))
        {
            log_out("Could not get CC values.");
            return;
        }
        log_out("CC1 = %"PRIu32".%03"PRIu32" A", value_1.u32/1000, value_1.u32%1000);
        log_out("CC2 = %"PRIu32".%03"PRIu32" A", value_2.u32/1000, value_2.u32%1000);
        log_out("CC3 = %"PRIu32".%03"PRIu32" A", value_3.u32/1000, value_3.u32%1000);
        return;
    }
    if (cc_num > 3 || cc_num == 0)
    {
        log_out("cc [1/2/3]");
        return;
    }
    char name[4];
    snprintf(name, 4, "CC%"PRIu8, cc_num);
    if (!cc_get_blocking(name, &value_1))
    {
        log_out("Could not get adc value.");
        return;
    }

    char temp[8] = "";

    value_to_str(&value_1, temp, sizeof(temp));

    log_out("CC = %smA", temp);
}


static void cc_calibrate_cb(char *args)
{
    cc_calibrate();
}


static void cc_mp_cb(char* args)
{
    // 2046 CC1
    char* p;
    float new_mp = strtof(args, &p);
    p = skip_space(p);
    uint32_t new_mp32;
    if (p == args)
    {
        cc_get_midpoint(&new_mp32, p);
        log_out("MP: %"PRIu32".%03"PRIu32, new_mp32/1000, new_mp32%1000);
        return;
    }
    new_mp32 = new_mp * 1000;
    p = skip_space(p);
    if (!cc_set_midpoint(new_mp32, p))
        log_out("Failed to set the midpoint.");
}


static void cc_gain(char* args)
{
    // <index> <ext_A> <int_mV>
    // 1       100     50
    cc_config_t* cc_conf = persist_get_cc_configs();
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    p = skip_space(p);
    if (strlen(p) == 0)
    {
        for (uint8_t i = 0; i < ADC_CC_COUNT; i++)
        {
            log_out("CC%"PRIu8" EXT max: %"PRIu32".%03"PRIu32"A", i+1, cc_conf[i].ext_max_mA/1000, cc_conf[i].ext_max_mA%1000);
            log_out("CC%"PRIu8" INT max: %"PRIu32".%03"PRIu32"V", i+1, cc_conf[i].int_max_mV/1000, cc_conf[i].int_max_mV%1000);
        }
        return;
    }
    if (index == 0 || index > ADC_CC_COUNT + 1 || p == args)
        goto syntax_exit;
    index--;

    char* q;
    float ext_A = strtof(p, &q);
    if (q == p)
        goto print_exit;
    q = skip_space(q);
    uint32_t int_mA = strtoul(q, &p, 10);
    if (p == q)
        goto syntax_exit;
    cc_conf[index].ext_max_mA = ext_A * 1000;
    cc_conf[index].int_max_mV = int_mA;
    log_out("Set the CC gain:");
print_exit:
    log_out("EXT max: %"PRIu32".%03"PRIu32"A", cc_conf[index].ext_max_mA/1000, cc_conf[index].ext_max_mA%1000);
    log_out("INT max: %"PRIu32".%03"PRIu32"V", cc_conf[index].int_max_mV/1000, cc_conf[index].int_max_mV%1000);
    return;
syntax_exit:
    log_out("Syntax: cc_gain <channel> <ext max A> <ext min mV>");
    log_out("e.g cc_gain 3 100 50");
}


static void ds18b20_cb(char* args)
{
    float ds18b20_temp;
    if (!ds18b20_query_temp(&ds18b20_temp, args))
    {
        log_error("Could not get a temperature from the onewire.");
        return;
    }
    log_out("Temp: %.03f degC.", ds18b20_temp);
}


static void timer_cb(char* args)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    log_out("Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
}


static void temperature_cb(char* args)
{
    int32_t temp;
    if (htu21d_read_temp(&temp))
        log_out("Temperature: %0.3fdegC", (float)temp/100.f);
}


static void humidity_cb(char* args)
{
    int32_t humi;
    if (htu21d_read_humidity(&humi))
        log_out("Humidity: %0.3f%%", (float)humi/100.f);
}


static void dew_point_cb(char* args)
{
    int32_t dew_temp;
    htu21d_read_dew_temp(&dew_temp);
    log_out("Dew temperature: %0.3fdegC", (float)dew_temp/100.f);
}


static void lora_conn_cb(char* args)
{
    if (lw_get_connected())
    {
        log_out("1 | Connected");
    }
    else
    {
        log_out("0 | Disconnected");
    }
}


static void wipe_cb(char* args)
{
    persistent_wipe();
}


static void interval_mins_cb(char* args)
{
    if (args[0])
    {
        uint32_t new_interval_mins = strtoul(args, NULL, 10);
        if (!new_interval_mins)
            new_interval_mins = 5;

        log_out("Setting interval minutes to %"PRIu32, new_interval_mins);
        persist_set_mins_interval(new_interval_mins);
        transmit_interval = new_interval_mins;
    }
    else
    {
        log_out("Current interval minutes is %"PRIu32, transmit_interval);
    }
}


static void bat_cb(char* args)
{
    value_t value;
    if (!bat_get_blocking(NULL, &value))
    {
        log_out("Could not get bat value.");
        return;
    }

    log_out("Bat %u.%02u", value.u16 / 100, value.u16 %100);
}


static void lw_dbg_cb(char* args)
{
    uart_ring_out(LW_UART, args, strlen(args));
    uart_ring_out(LW_UART, "\r\n", 2);
}


static void light_cb(char* args)
{
    uint32_t lux;
    if (!veml7700_get_lux(&lux))
    {
        log_out("Could not get light level");
        return;
    }
    log_out("Lux: %"PRIu32, lux);
}


static void sound_cb(char* args)
{
    uint32_t dB;
    if (!sai_get_sound(&dB))
    {
        log_out("Can not get the sound.");
        return;
    }
    log_out("Sound = %"PRIu32".%"PRIu32" dB", dB/10, dB%10);
}


static void sound_cal_cb(char* args)
{
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    if (index < 1 || index > SAI_NUM_CAL_COEFFS)
    {
        log_out("Index out of range.");
        return;
    }
    p = skip_space(p);
    float coeff = strtof(p, NULL);
    if (!sai_set_coeff(index-1, coeff))
        log_out("Could not set the coefficient.");
}


static void repop_cb(char* args)
{
    measurements_repopulate();
    log_out("Repopulated measurements.");
}


static void no_lw_cb(char* args)
{
    bool enable = strtoul(args, NULL, 10);
    measurements_set_debug_mode(enable);
}


void sleep_cb(char* args)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        log_out("<TIME(MS)>");
        return;
    }
    log_out("Sleeping for %"PRIu32"ms.", sleep_ms);
    sleep_for_ms(sleep_ms);
}


void power_mode_cb(char* args)
{
    measurements_power_mode_t mode;
    if (args[0] == 'A')
        mode = MEASUREMENTS_POWER_MODE_AUTO;
    else if (args[0] == 'B')
        mode = MEASUREMENTS_POWER_MODE_BATTERY;
    else if (args[0] == 'P')
        mode = MEASUREMENTS_POWER_MODE_PLUGGED;
    else
        return;
    measurements_power_mode(mode);
}


static void can_impl_cb(char* args)
{
    can_impl_send_example();
}


static void debug_mode_cb(char* args)
{
    uint32_t mask = persist_get_log_debug_mask();
    if (mask & DEBUG_MODE)
    {
        debug_mode(); /* Toggle it off.*/
        persist_set_log_debug_mask(mask & ~DEBUG_MODE);
        return;
    }
    persist_set_log_debug_mask(mask | DEBUG_MODE);
    platform_raw_msg("Rebooting in debug_mode.");
    reset_cb(args);
}


static void serial_num_cb(char* args)
{
    char* serial_num = persist_get_serial_number();
    char* p = skip_space(args);
    char dev_eui[LW_DEV_EUI_LEN + 1];
    uint8_t len = strnlen(p, SERIAL_NUM_LEN);
    if (len == 0)
        goto print_exit;
    log_out("Updating serial number.");
    strncpy(serial_num, p, len);
    serial_num[len] = 0;
print_exit:
    if (!persist_get_lw_dev_eui(dev_eui))
    {
        log_out("%s", persist_get_serial_number());
        return;
    }
    log_out("Serial Number: %s-%s", serial_num, dev_eui);
    return;
}


void cmds_process(char * command, unsigned len)
{
    static cmd_t cmds[] = {
        { "ios",          "Print all IOs.",           ios_log},
        { "io",           "Get/set IO set.",          io_cb},
        { "en_pulse",     "Enable Pulsecount IO.",    cmd_enable_pulsecount_cb},
        { "en_w1",        "Enable OneWire IO.",       cmd_enable_onewire_cb},
        { "count",        "Counts of controls.",      count_cb},
        { "version",      "Print version.",           version_cb},
        { "lora",         "Send lora message",        lora_cb},
        { "lora_config",  "Set lora config",          lora_config_cb},
        { "interval",     "Set the interval",         interval_cb},
        { "samplecount",  "Set the samplecount",      samplecount_cb},
        { "debug",        "Set hex debug mask",       debug_cb},
        { "hpm",          "Enable/Disable HPM",       hpm_cb},
        { "mb_setup",     "Change Modbus comms",      modbus_setup_cb},
        { "mb_dev_add",   "Add modbus dev",           modbus_add_dev_cb},
        { "mb_reg_add",   "Add modbus reg",           modbus_add_reg_cb},
        { "mb_get_reg",   "Get modbus reg",           modbus_get_reg_cb},
        { "mb_reg_del",   "Delete modbus reg",        modbus_measurement_del_reg},
        { "mb_dev_del",   "Delete modbus dev",        modbus_measurement_del_dev},
        { "mb_log",       "Show modbus setup",        modbus_log},
        { "save",         "Save config",              persist_commit},
        { "measurements", "Print measurements",       measurements_cb},
        { "fw+",          "Add chunk of new fw.",     fw_add},
        { "fw@",          "Finishing crc of new fw.", fw_fin},
        { "reset",        "Reset device.",            reset_cb},
        { "cc",           "CC value",                 cc_cb},
        { "cc_cal",       "Calibrate the cc",         cc_calibrate_cb},
        { "cc_mp",        "Set the CC midpoint",      cc_mp_cb},
        { "cc_gain",      "Set the max int and ext",  cc_gain},
        { "w1",           "Get temperature with w1",  ds18b20_cb},
        { "timer",        "Test usecs timer",         timer_cb},
        { "temp",         "Get the temperature",      temperature_cb},
        { "humi",         "Get the humidity",         humidity_cb},
        { "dew",          "Get the dew temperature",  dew_point_cb},
        { "lora_conn",    "LoRa connected",           lora_conn_cb},
        { "wipe",         "Factory Reset",            wipe_cb},
        { "interval_mins","Get/Set interval minutes", interval_mins_cb},
        { "bat",          "Get battery level.",       bat_cb},
        { "pulsecount",   "Show pulsecount.",         pulsecount_log},
        { "lw_dbg",       "LoraWAN Chip Debug",       lw_dbg_cb},
        { "light",        "Get the light in lux.",    light_cb},
        { "sound",        "Get the sound in lux.",    sound_cb},
        { "cal_sound",    "Set the cal coeffs.",      sound_cal_cb},
        { "repop",        "Repopulate measurements.", repop_cb},
        { "no_lw",        "Dont need LW for measurements", no_lw_cb},
        { "sleep",        "Sleep",                    sleep_cb},
        { "power_mode",   "Power mode setting",       power_mode_cb},
        { "can_impl",     "Send example CAN message", can_impl_cb},
        { "debug_mode",   "Set/unset debug mode",     debug_mode_cb},
        { "serial_num",   "Set/get serial number",    serial_num_cb},
        { NULL },
    };

    if (!len)
        return;

    log_sys_debug("Command \"%s\"", command);

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
