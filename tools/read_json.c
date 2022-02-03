#include "json_x_img.h"


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

    const char * str = json_object_get_string(tmp);
    unsigned str_len = strlen(str);

    if (str_len > len)
        return false;

    memcpy(buf, str, str_len);
    if (len > str_len)
        buf[str_len] = 0;

    return true;
}


static void _measurement_del(const char * name)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        char * def_name = osm_config.measurements_arr[i].name;
        if (strcmp(name, def_name) == 0)
        {
            def_name[0] = 0;
            return;
        }
    }
}


measurement_def_t* _measurement_get_free(measurement_def_t * measurement_arr)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
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

    log_error("Unknown IO direction \"%s\", default to as was.", str);
    return current;
}


static bool _read_modbus_json(struct json_object * root)
{
    struct json_object * modbus_bus = json_object_object_get(root, "modbus_bus");
    if (modbus_bus)
    {
        osm_config.modbus_bus.version     = MODBUS_BLOB_VERSION;
        osm_config.modbus_bus.max_dev_num = MODBUS_MAX_DEV;
        osm_config.modbus_bus.max_reg_num = MODBUS_DEV_REGS;
        osm_config.modbus_bus.binary_protocol = (uint8_t)json_object_get_boolean(json_object_object_get(modbus_bus, "binary_protocol"));

        char con_str[16];

        if (_get_string_buf(modbus_bus, "con_str", con_str, sizeof(con_str)))
        {
            uart_parity_t parity;
            uart_stop_bits_t stopbits;

            if (!decompose_uart_str(con_str,
                               &osm_config.modbus_bus.baudrate,
                               &osm_config.modbus_bus.databits,
                               &parity,
                               &stopbits))
            {
                log_error("Invalid connection string.");
                return false;
            }

            osm_config.modbus_bus.parity   = parity;
            osm_config.modbus_bus.stopbits = stopbits;
        }
        else
        {
            osm_config.modbus_bus.baudrate = UART_4_SPEED;
            osm_config.modbus_bus.databits = UART_4_DATABITS;
            osm_config.modbus_bus.parity   = UART_4_PARITY;
            osm_config.modbus_bus.stopbits = UART_4_STOP;
        }

        struct json_object * devices = json_object_object_get(root, "modbus_devices");
        if (devices)
        {
            json_object_object_foreach(devices, dev_name, dev_node)
            {
                if (strlen(dev_name) > MODBUS_NAME_LEN)
                {
                    log_error("Modbus device name \"%s\" too long.", dev_name);
                    return false;
                }

                int unit_id =  _get_defaulted_int(dev_node, "unit_id", 0);
                if (unit_id < 1 || unit_id > 255)
                {
                    log_error("Invalid unit ID for modbus device %s", dev_name);
                    return false;
                }

                modbus_dev_t * dev = modbus_add_device(unit_id, dev_name);

                if (!dev)
                    return false;

                struct json_object * registers = json_object_object_get(dev_node, "registers");
                if (registers)
                {
                    unsigned reg_index = 0;
                    json_object_object_foreach(registers, reg_name, reg_node)
                    {
                        if (strlen(reg_name) > MODBUS_NAME_LEN)
                        {
                            log_error("Modbus register name \"%s\" too long.", reg_name);
                            return false;
                        }

                        modbus_reg_t * reg = &dev->regs[reg_index++];

                        struct json_object * reg_type = json_object_object_get(reg_node, "type");
                        if (!reg_type)
                        {
                            log_error("No reg type for modbus register %s", reg_name);
                            return false;
                        }

                        modbus_reg_type_t type     = modbus_reg_type_from_str(json_object_get_string(reg_type), NULL);
                        int               reg_addr = _get_defaulted_int(dev_node, "reg", 0);
                        int               func     = _get_defaulted_int(dev_node, "func", 0);

                        if (!modbus_dev_add_reg(dev, reg_name, type, (uint8_t)func, (uint16_t)reg_addr))
                            return false;

                        _measurement_del(reg_name);

                        measurement_def_t * def = _measurement_get_free(osm_config.measurements_arr);
                        if (!def)
                            return false;
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
    return true;
}


static bool _read_measurements_json(struct json_object * root)
{
    struct json_object * measurements_node = json_object_object_get(root, "measurements");
    if (measurements_node)
    {
        json_object_object_foreach(measurements_node, measurement_name, measurement_node)
        {
            measurement_def_t * def = measurements_array_find(osm_config.measurements_arr, measurement_name);
            if (!def)
            {
                log_error("Unknown measurement \"%s\"", measurement_name);
                return false;
            }

            int interval = _get_defaulted_int(measurement_node, "interval", 1);
            if (interval < 0 || interval > 255)
            {
                log_error("Bad interval for measurement \"%s\"", measurement_name);
                return false;
            }
            int samplecount = _get_defaulted_int(measurement_node, "samplecount", 1);
            if (samplecount < 0 || samplecount > 255)
            {
                log_error("Bad samplecount for measurement \"%s\"", measurement_name);
                return false;
            }
            def->interval    = (uint8_t)interval;
            def->samplecount = (uint8_t)samplecount;
        }
    }

    return true;
}


static bool _read_ios_json(struct json_object * root)
{
    {
        uint16_t ios_init_state[IOS_COUNT] = IOS_STATE;
        memcpy(osm_config.ios_state, ios_init_state, sizeof(ios_init_state));
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
                return false;
            }
            uint16_t state = osm_config.ios_state[index];
            uint16_t pull    = _get_io_pull(json_object_get_string(json_object_object_get(io_node, "pull")), state & IO_PULL_MASK);
            uint16_t dir     = _get_io_dir(json_object_get_string(json_object_object_get(io_node, "direction")), state & IO_AS_INPUT);
            uint16_t special = (json_object_get_boolean(json_object_object_get(io_node, "use_special")))?IO_SPECIAL_EN:0;
            if (special && (!(state & IO_TYPE_MASK)))
            {
                log_error("IO %s has no special", io_name);
                return false;
            }

            state &= ~(IO_PULL_MASK|IO_AS_INPUT|IO_SPECIAL_EN);

            if (!dir)
                state |= (json_object_get_boolean(json_object_object_get(io_node, "out_high")))?IO_OUT_ON:0;

            osm_config.ios_state[index] = state | (pull | dir | special);
            index++;
        }
    }

    return true;
}


int read_json_to_img(const char * filename)
{
    struct json_object * root = json_object_from_fd(0);
    if (!root)
    {
        log_error("Failed to read json : %s", json_util_get_last_err());
        return EXIT_FAILURE;
    }

    struct json_object * obj = json_object_object_get(root, "version");
    if (!obj)
    {
        log_error("No version given.");
        goto bad_exit;
    }

    memset(&osm_config, 0, sizeof(osm_config));

    osm_config.version = json_object_get_int(obj);
    if (osm_config.version != PERSIST__VERSION)
    {
        log_error("Wrong version.");
        goto bad_exit;
    }

    osm_config.log_debug_mask = _get_defaulted_int(root, "log_debug_mask", DEBUG_SYS);
    int tmp = _get_defaulted_int(root, "mins_interval",  15);
    if (tmp < 1 || tmp > (60 * 24))
    {
        log_error("Unsupported mins_interval");
        goto bad_exit;
    }
    osm_config.mins_interval  = tmp;
    tmp = _get_defaulted_int(root, "cc_midpoint",    ADC_MAX_VAL/2);
    if (tmp < 0 || tmp > ADC_MAX_VAL)
    {
        log_error("Unsupported cc_midpoint");
        goto bad_exit;
    }
    osm_config.adc_midpoint   = tmp;

    if (!_get_string_buf(root, "lw_dev_eui", osm_config.lw_dev_eui, LW__DEV_EUI_LEN) ||
        !_get_string_buf(root, "lw_app_key", osm_config.lw_app_key, LW__APP_KEY_LEN))
    {
        log_error("No LoRaWAN keys.");
        goto bad_exit;
    }

    measurements_add_defaults(osm_config.measurements_arr);

    if (!_read_modbus_json(root))
        goto bad_exit;

    if (!_read_measurements_json(root))
        goto bad_exit;

    if (!_read_ios_json(root))
        goto bad_exit;

    FILE * f = fopen(filename,"w");

    if (!f)
    {
        perror("Failed to open file.");
        goto bad_exit;
    }

    if (fwrite(&osm_config, sizeof(osm_config), 1, f) != 1)
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
