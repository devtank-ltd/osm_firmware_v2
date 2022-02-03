#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <json-c/json.h>
#include <json-c/json_util.h>

#include "pinmap.h"
#include "modbus_mem.h"

#include "persist_config_header.h"

#define GPIO_PUPD_NONE     0
#define GPIO_PUPD_PULLUP   1
#define GPIO_PUPD_PULLDOWN 2


static persist_storage_t _config = {.version = PERSIST__VERSION,
                                    .log_debug_mask = DEBUG_SYS};

static void print_help(void)
{
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "filename: (config img filename)\n");
    exit(EXIT_FAILURE);
}


extern void log_debug(uint32_t flag, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("DEBUG: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}


extern void log_out(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}


extern void log_error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr,"\n");
    va_end(ap);
}


extern modbus_bus_t* persist_get_modbus_bus(void)
{
    return &_config.modbus_bus;
}

static char _input_buffer[1024*1024];


static int _read_config_img(const char * filename)
{
    FILE * f = fopen(filename,"r");

    if (!f)
    {
        perror("Failed to open file.");
        return EXIT_FAILURE;
    }
    struct json_object * root = json_object_new_object();
    if (fread(&_config, sizeof(_config), 1, f) != 1)
    {
        perror("Failed to read file.");
        json_object_put(root);
        fclose(f);
        return EXIT_FAILURE;
    }

    json_object_object_add(root, "version", json_object_new_int(PERSIST__VERSION));

    json_object_object_add(root, "log_debug_mask", json_object_new_int(DEBUG_SYS));

    json_object_object_add(root, "mins_interval", json_object_new_int(15));

    json_object_object_add(root, "lw_dev_eui", json_object_new_string_len(_config.lw_dev_eui, LW__DEV_EUI_LEN));
    json_object_object_add(root, "lw_app_key", json_object_new_string_len(_config.lw_app_key, LW__APP_KEY_LEN));

    modbus_bus_t* bus = &_config.modbus_bus;

    if (bus->version == MODBUS_BLOB_VERSION &&
        bus->max_dev_num == MODBUS_MAX_DEV &&
        bus->max_reg_num == MODBUS_DEV_REGS)
    {
        modbus_log();
    }

    json_object_to_fd(1, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    fclose(f);

    return EXIT_SUCCESS;
}


static int _get_defaulted_int(struct json_object * root, char * name, int default_value)
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return default_value;
    return json_object_get_int(tmp);
}

static bool _get_string_buf(struct json_object * root, char * name, char * buf, unsigned len)
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return false;
    snprintf(buf, len, json_object_get_string(tmp));
    return true;
}


static void _measurement_del(const char * name)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        char * def_name = _config.measurements_arr[i].name;
        if (strcmp(name, def_name) == 0)
        {
            def_name[0] = 0;
            return;
        }
    }
}


measurement_def_t* _measurement_get_free(measurement_def_t * measurement_arr)
{
    for (int i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_t * def = &measurement_arr[i];
        if (!def->name[0])
            return def;
    }
    log_error("No free measurement slots.");
    return NULL;
}


static uint16_t _get_io_pull(const char * str, uint16_t current)
{
    if (!str)
        return current;

    if (strcasecmp(str, "UP") == 0)
        return GPIO_PUPD_PULLUP;

    if (strcasecmp(str, "DOWN") == 0)
        return GPIO_PUPD_PULLDOWN;

    if (strcasecmp(str, "NONE") == 0)
        return GPIO_PUPD_NONE;
    log_error("Unknown IO pull, default to as was.");
    return current;
}


static uint16_t _get_io_dir(const char * str, uint16_t current)
{
    if (!str)
        return current;

    if (strcasecmp(str, "IN") == 0)
        return IO_AS_INPUT;

    if (strcasecmp(str, "OUT") == 0)
        return 0;

    log_error("Unknown IO direction, default to as was.");
    return current;
}


static int _write_config_img(const char * filename)
{
    struct json_object * root = json_object_from_fd(0);
    if (root)
    {
        perror("Failed to read json.");
        return EXIT_FAILURE;
    }

    struct json_object * obj = json_object_object_get(root, "version");
    if (!obj)
    {
        log_error("No version given.");
        goto bad_exit;
    }

    memset(&_config, 0, sizeof(_config));

    _config.version = json_object_get_int(obj);
    if (_config.version != PERSIST__VERSION)
    {
        log_error("Wrong version.");
        goto bad_exit;
    }

    _config.log_debug_mask = _get_defaulted_int(root, "log_debug_mask", DEBUG_SYS);
    int tmp = _get_defaulted_int(root, "mins_interval",  15);
    if (tmp < 1 || tmp > (60 * 24))
    {
        log_error("Unsupported mins_interval");
        goto bad_exit;
    }
    _config.mins_interval  = tmp;
    tmp = _get_defaulted_int(root, "cc_midpoint",    ADC_MAX_VAL/2);
    if (tmp < 0 || tmp > ADC_MAX_VAL)
    {
        log_error("Unsupported cc_midpoint");
        goto bad_exit;
    }
    _config.adc_midpoint   = tmp;

    if (!_get_string_buf(root, "lw_dev_eui", _config.lw_dev_eui, LW__DEV_EUI_LEN) ||
        !_get_string_buf(root, "lw_app_key", _config.lw_app_key, LW__APP_KEY_LEN))
    {
        log_error("No LoRaWAN keys.");
        goto bad_exit;
    }

    measurements_add_defaults(_config.measurements_arr);

    struct json_object * modbus_bus = json_object_object_get(root, "modbus_bus");
    if (modbus_bus)
    {
        _config.modbus_bus.version     = MODBUS_BLOB_VERSION;
        _config.modbus_bus.max_dev_num = MODBUS_MAX_DEV;
        _config.modbus_bus.max_reg_num = MODBUS_DEV_REGS;
        _config.modbus_bus.binary_protocol = (uint8_t)json_object_get_boolean(json_object_object_get(modbus_bus, "binary_protocol"));

        char con_str[16];

        if (_get_string_buf(modbus_bus, "con_str", con_str, sizeof(con_str)))
        {
            uart_parity_t parity;
            uart_stop_bits_t stopbits;

            if (!decompose_uart_str(con_str,
                               &_config.modbus_bus.baudrate,
                               &_config.modbus_bus.databits,
                               &parity,
                               &stopbits))
            {
                log_error("Invalid connection string.");
                goto bad_exit;
            }

            _config.modbus_bus.parity   = parity;
            _config.modbus_bus.stopbits = stopbits;
        }
        else
        {
            _config.modbus_bus.baudrate = UART_4_SPEED;
            _config.modbus_bus.databits = UART_4_DATABITS;
            _config.modbus_bus.parity   = UART_4_PARITY;
            _config.modbus_bus.stopbits = UART_4_STOP;
        }

        struct json_object * devices = json_object_object_get(root, "modbus_devices");
        if (devices)
        {
            json_object_object_foreach(devices, dev_name, dev_node)
            {
                if (strlen(dev_name) > MODBUS_NAME_LEN)
                {
                    log_error("Modbus device name \"%s\" too long.", dev_name);
                    goto bad_exit;
                }

                int unit_id =  _get_defaulted_int(dev_node, "unit_id", 0);
                if (unit_id < 1 || unit_id > 255)
                {
                    log_error("Invalid unit ID for modbus device %s", dev_name);
                    goto bad_exit;
                }

                modbus_dev_t * dev = modbus_add_device(unit_id, dev_name);

                if (!dev)
                    goto bad_exit;

                struct json_object * registers = json_object_object_get(dev_node, "registers");
                if (registers)
                {
                    unsigned reg_index = 0;
                    json_object_object_foreach(registers, reg_name, reg_node)
                    {
                        if (strlen(reg_name) > MODBUS_NAME_LEN)
                        {
                            log_error("Modbus register name \"%s\" too long.", reg_name);
                            goto bad_exit;
                        }

                        modbus_reg_t * reg = &dev->regs[reg_index++];

                        struct json_object * reg_type = json_object_object_get(reg_node, "type");
                        if (!reg_type)
                        {
                            log_error("No reg type for modbus register %s", reg_name);
                            goto bad_exit;
                        }

                        modbus_reg_type_t type     = modbus_reg_type_from_str(json_object_get_string(reg_type), NULL);
                        int               reg_addr = _get_defaulted_int(dev_node, "reg", 0);
                        int               func     = _get_defaulted_int(dev_node, "func", 0);

                        if (!modbus_dev_add_reg(dev, reg_name, type, (uint8_t)func, (uint16_t)reg_addr))
                            goto bad_exit;

                        _measurement_del(reg_name);

                        measurement_def_t * def = _measurement_get_free(_config.measurements_arr);
                        if (!def)
                            goto bad_exit;
                        memcpy(def->name, reg_name, MODBUS_NAME_LEN);
                        def->name[MODBUS_NAME_LEN] = 0;
                        def->samplecount = 1;
                        def->interval    = 1;
                        def->type        = MODBUS;
                    }
                }
            }
        }
        log_out("Loaded modbus.");
        modbus_log();
    }

    struct json_object * measurements_node = json_object_object_get(root, "measurements");
    if (measurements_node)
    {
        json_object_object_foreach(measurements_node, measurement_name, measurement_node)
        {
            measurement_def_t * def = measurements_array_find(_config.measurements_arr, measurement_name);
            if (!def)
            {
                log_error("Unknown measurement \"%s\"", measurement_name);
                goto bad_exit;
            }

            int interval = _get_defaulted_int(measurement_node, "interval", 1);
            if (interval < 0 || interval > 255)
            {
                log_error("Bad interval for measurement \"%s\"", measurement_name);
                goto bad_exit;
            }
            int samplecount = _get_defaulted_int(measurement_node, "samplecount", 1);
            if (samplecount < 0 || samplecount > 255)
            {
                log_error("Bad samplecount for measurement \"%s\"", measurement_name);
                goto bad_exit;
            }
            def->interval    = (uint8_t)interval;
            def->samplecount = (uint8_t)samplecount;
        }
    }

    {
        uint16_t ios_init_state[IOS_COUNT] = IOS_STATE;
        memcpy(_config.ios_state, ios_init_state, sizeof(ios_init_state));
    }

    struct json_object * ios_node = json_object_object_get(root, "ios");
    if (ios_node)
    {
        unsigned index = 0;
        json_object_object_foreach(ios_node, io_name, io_node)
        {
            if (index >= IOS_COUNT)
            {
                log_error("Too many IOs");
                goto bad_exit;
            }
            uint16_t state = _config.ios_state[index];
            uint16_t pull    = _get_io_pull(json_object_get_string(json_object_object_get(io_node, "pull")), state & IO_PULL_MASK);
            uint16_t dir     = _get_io_dir(json_object_get_string(json_object_object_get(io_node, "direction")), state & IO_AS_INPUT);
            uint16_t special = (json_object_get_boolean(json_object_object_get(io_node, "use_special")))?IO_SPECIAL_EN:0;
            _config.ios_state[index] = (state & ~(IO_PULL_MASK|IO_AS_INPUT|IO_SPECIAL_EN)) | (pull | dir | special);
        }
    }

    FILE * f = fopen(filename,"w");

    if (!f)
    {
        perror("Failed to open file.");
        goto bad_exit;
    }

    if (fwrite(&_config, sizeof(_config), 1, f) != 1)
    {
        perror("Failed to write file.");
        fclose(f);
        goto bad_exit;
    }

    json_object_put(root);
    fclose(f);

    return EXIT_SUCCESS;

bad_exit:
    json_object_put(root);
    return EXIT_FAILURE;
}



int main(int argc, char *argv[])
{
    if (argc < 2)
        print_help();

    const char * filename   = argv[1];

    if (isatty(0))
        return _read_config_img(filename);
    else
        return _write_config_img(filename);
}
