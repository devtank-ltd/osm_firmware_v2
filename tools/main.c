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

    _config.log_debug_mask =  _get_defaulted_int(root, "log_debug_mask", DEBUG_SYS);
    _config.mins_interval =  _get_defaulted_int(root, "mins_interval", 15);
    if (!_get_string_buf(root, "lw_dev_eui", _config.lw_dev_eui, LW__DEV_EUI_LEN) ||
        !_get_string_buf(root, "lw_app_key", _config.lw_app_key, LW__APP_KEY_LEN))
    {
        log_error("No LoRaWAN keys.");
        goto bad_exit;
    }

    struct json_object * modbus_bus = json_object_object_get(root, "modbus_bus");
    if (modbus_bus)
    {
        _config.modbus_bus.version     = MODBUS_BLOB_VERSION;
        _config.modbus_bus.max_dev_num = MODBUS_MAX_DEV;
        _config.modbus_bus.max_reg_num = MODBUS_DEV_REGS;


/*binary_protocol*/


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
            unsigned dev_index = 0;

            json_object_object_foreach(devices, dev_name, dev_node)
            {
                if (strlen(dev_name) > MODBUS_NAME_LEN)
                {
                    log_error("Modbus device name \"%s\" too long.", dev_name);
                    goto bad_exit;
                }

                modbus_dev_t * dev = &_config.modbus_bus.modbus_devices[dev_index++];

                int unit_id =  _get_defaulted_int(dev_node, "unit_id", 0);
                if (unit_id < 1)
                {
                    log_error("Invalid unit ID for modbus device %s", dev_name);
                    goto bad_exit;
                }

                dev->slave_id = unit_id;

                snprintf(dev->name, MODBUS_NAME_LEN, dev_name);

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

                        reg->type = modbus_reg_type_from_str(json_object_get_string(reg_type));
                        if (reg->type = MODBUS_REG_TYPE_INVALID)
                        {
                            log_error("Invalid reg type for modbus register %s", reg_name);
                            goto bad_exit;
                        }

                        snprintf(reg->name, MODBUS_NAME_LEN, reg_name);

                        int reg_addr = _get_defaulted_int(dev_node, "reg", 0);
                        reg->reg_addr = reg_addr;

                        int func = _get_defaulted_int(dev_node, "func", 0);
                        if (func != 3 && func != 4)
                        {
                            log_error("Modbus register %s function %d not supported", reg_name, func);
                            goto bad_exit;
                        }

                        reg->func = func;
                    }
                }
            }
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
