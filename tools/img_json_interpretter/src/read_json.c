#include "json_x_img.h"


#define WORD_BYTE_ORDER_TXT_SIZE            3


static int _get_defaulted_int(struct json_object * root, char * name, int default_value)
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return default_value;
    return json_object_get_int(tmp);
}


static double _get_defaulted_double(struct json_object * root, char * name, float default_value)
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return default_value;
    return json_object_get_double(tmp);
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
        char * def_name = osm_mem.measurements->measurements_arr[i].name;
        if (strcmp(name, def_name) == 0)
        {
            def_name[0] = 0;
            return;
        }
    }
}


measurements_def_t* _measurement_get_free(measurements_def_t * measurement_arr)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t * def = &measurement_arr[i];
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


static bool _read_modbus_json(struct json_object * root, modbus_bus_t* modbus_bus)
{
    if (!root || !modbus_bus)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }
    struct json_object * modbus_bus_json = json_object_object_get(root, "modbus_bus");
    if (modbus_bus_json)
    {
        modbus_bus->version     = MODBUS_BLOB_VERSION;
        modbus_bus->binary_protocol = (uint8_t)json_object_get_boolean(json_object_object_get(modbus_bus_json, "binary_protocol"));

        char con_str[16];

        if (_get_string_buf(modbus_bus_json, "con_str", con_str, sizeof(con_str)))
        {
            uart_parity_t parity;
            uart_stop_bits_t stopbits;
            uint8_t databits;;


            if (!decompose_uart_str(con_str,
                               &modbus_bus->baudrate,
                               &databits,
                               &parity,
                               &stopbits))
            {
                log_error("Invalid connection string.");
                return false;
            }

            modbus_bus->databits = databits;
            modbus_bus->parity   = parity;
            modbus_bus->stopbits = stopbits;
        }
        else
        {
            modbus_bus->baudrate = UART_4_SPEED;
            modbus_bus->databits = UART_4_DATABITS;
            modbus_bus->parity   = UART_4_PARITY;
            modbus_bus->stopbits = UART_4_STOP;
        }

        struct json_object * devices = json_object_object_get(modbus_bus_json, "modbus_devices");
        if (devices)
        {
            json_object_object_foreach(devices, dev_name, dev_node)
            {
                if (strlen(dev_name) > MODBUS_NAME_LEN)
                {
                    log_error("Modbus device name \"%s\" too long.", dev_name);
                    return false;
                }

                char w_b_order_txt[WORD_BYTE_ORDER_TXT_SIZE];
                if (!_get_string_buf(dev_node, "word_order", w_b_order_txt, WORD_BYTE_ORDER_TXT_SIZE))
                {
                    log_error("Could not interpret word order.");
                    return false;
                }
                uint8_t word_order;
                if (strncmp(w_b_order_txt, "LSW", WORD_BYTE_ORDER_TXT_SIZE) == 0)
                    word_order = MODBUS_WORD_ORDER_LSW;
                else if (strncmp(w_b_order_txt, "MSW", WORD_BYTE_ORDER_TXT_SIZE) == 0)
                    word_order = MODBUS_WORD_ORDER_MSW;
                else
                {
                    log_error("Word order not recognised.");
                    return false;
                }
                if (!_get_string_buf(dev_node, "byte_order", w_b_order_txt, WORD_BYTE_ORDER_TXT_SIZE))
                {
                    log_error("Could not interpret byte order.");
                    return false;
                }
                uint8_t byte_order;
                if (strncmp(w_b_order_txt, "LSB", WORD_BYTE_ORDER_TXT_SIZE) == 0)
                    byte_order = MODBUS_WORD_ORDER_LSW;
                else if (strncmp(w_b_order_txt, "MSB", WORD_BYTE_ORDER_TXT_SIZE) == 0)
                    byte_order = MODBUS_WORD_ORDER_MSW;
                else
                {
                    log_error("Byte order not recognised.");
                    return false;
                }

                int unit_id =  _get_defaulted_int(dev_node, "unit_id", 0);
                if (unit_id < 1 || unit_id > 255)
                {
                    log_error("Invalid unit ID for modbus device %s", dev_name);
                    return false;
                }

                modbus_dev_t * dev = modbus_add_device(unit_id, dev_name, byte_order, word_order);

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

                        struct json_object * reg_type = json_object_object_get(reg_node, "type");
                        if (!reg_type)
                        {
                            log_error("No reg type for modbus register %s", reg_name);
                            return false;
                        }

                        modbus_reg_type_t type     = modbus_reg_type_from_str(json_object_get_string(reg_type), NULL);
                        int               reg_addr = _get_defaulted_int(reg_node, "reg", 0);
                        int               func     = _get_defaulted_int(reg_node, "func", 0);

                        if (!modbus_dev_add_reg(dev, reg_name, type, (uint8_t)func, (uint16_t)reg_addr))
                            return false;

                        _measurement_del(reg_name);

                        measurements_def_t * def = _measurement_get_free(osm_mem.measurements->measurements_arr);
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
            measurements_def_t * def = measurements_array_find(osm_mem.measurements->measurements_arr, measurement_name);
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


static bool _read_ios_json(struct json_object * root, uint16_t* ios_state)
{
    if (!root || !ios_state)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }
    {
        uint16_t ios_init_state[IOS_COUNT] = IOS_STATE;
        memcpy(ios_state, ios_init_state, sizeof(ios_init_state));
    }

    struct json_object * ios_node = json_object_object_get(root, "ios");
    if (ios_node)
    {
        json_object_object_foreach(ios_node, io_name, io_node)
        {
            unsigned index = strtoul(io_name, NULL, 10);
            if (index >= IOS_COUNT)
            {
                log_error("Too many IOs");
                return false;
            }
            uint16_t state = ios_state[index];
            uint16_t pull    = _get_io_pull(json_object_get_string(json_object_object_get(io_node, "pull")), state & IO_PULL_MASK);
            uint16_t dir     = _get_io_dir(json_object_get_string(json_object_object_get(io_node, "direction")), state & IO_AS_INPUT);
            uint16_t w1_mode = (json_object_get_boolean(json_object_object_get(io_node, "use_w1")))?IO_ONEWIRE:0;
            uint16_t pcnt_mode = (json_object_get_boolean(json_object_object_get(io_node, "use_pcnt")))?IO_PULSE:0;
            if (w1_mode && (!(state & IO_TYPE_MASK)))
            {
                log_error("IO %s has no w1 mode", io_name);
                return false;
            }

            if (pcnt_mode && (!(state & IO_TYPE_MASK)))
            {
                log_error("IO %s has no pulse counter mode", io_name);
                return false;
            }

            state &= ~(IO_PULL_MASK|IO_AS_INPUT|IO_ONEWIRE|IO_PULSE);

            if (!dir)
                state |= (json_object_get_boolean(json_object_object_get(io_node, "out_high")))?IO_OUT_ON:0;

            ios_state[index] = state | (pull | dir | w1_mode | pcnt_mode);
        }
    }

    return true;
}


static bool _read_cc_configs_json(struct json_object * root, cc_config_t* cc_config)
{
    if (!root || !cc_config)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }

    struct json_object * cc_configs_json = json_object_object_get(root, "cc_configs");
    if (cc_configs_json)
    {
        json_object_object_foreach(cc_configs_json, cc_config_name, cc_config_json)
        {
            char* p = cc_config_name;
            if (strncmp(p, "CC", 2) != 0)
            {
                log_error("Bad name midpoint for \"%s\"", cc_config_name);
                return false;
            }
            p += 2;
            int index = strtoul(p, NULL, 10);
            if (index < 1 || index > 3)
            {
                log_error("Bad name midpoint for \"%s\"", cc_config_name);
                return false;
            }
            double midpointf = _get_defaulted_double(cc_config_json, "midpoint", CC_DEFAULT_MIDPOINT / 1000.);
            midpointf *= 1000.;
            int midpoint = midpointf;
            if (midpoint < 0 || midpoint > UINT32_MAX)
            {
                log_error("Bad midpoint for \"%s\"", cc_config_name);
                return false;
            }
            cc_config[index-1].midpoint = midpoint;
            int ext_max_mA = _get_defaulted_int(cc_config_json, "ext_max_mA", CC_DEFAULT_EXT_MAX_MA);
            if (ext_max_mA < 0 || ext_max_mA > UINT32_MAX)
            {
                log_error("Bad external maximum mA.");
                return false;
            }
            cc_config[index-1].ext_max_mA = ext_max_mA;
            int int_max_mV = _get_defaulted_int(cc_config_json, "int_max_mV", CC_DEFAULT_INT_MAX_MV);
            if (int_max_mV < 0 || ext_max_mA > UINT32_MAX)
            {
                log_error("Bad internal maximum mV.");
                return false;
            }
            cc_config[index-1].int_max_mV = int_max_mV;
        }
    }
    return true;
}


static bool _read_ftma_configs_json(struct json_object * root, ftma_config_t* ftma_configs)
{
    if (!root || !ftma_configs)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }

    struct json_object * ftma_configs_json = json_object_object_get(root, "ftma_configs");
    if (ftma_configs_json)
    {
        json_object_object_foreach(ftma_configs_json, ftma_config_name, ftma_config_json)
        {
            char* p = ftma_config_name;
            if (strncmp(p, "FTA", 3) != 0)
            {
                log_error("Bad name for \"%s\"", ftma_config_name);
                return false;
            }
            p += 3;
            int index = strtoul(p, NULL, 10);
            if (index < 1 || index > 4)
            {
                log_error("Bad name for \"%s\"", ftma_config_name);
                return false;
            }
            ftma_config_t* ftma = &ftma_configs[index-1];
            if (!_get_string_buf(ftma_config_json, "name", ftma->name, MEASURE_NAME_LEN))
            {
                log_error("Invalid name for %s.", ftma_config_name);
                return false;
            }
            struct json_object * coeffs_json = json_object_object_get(ftma_config_json, "coeffs");
            int num_coeffs = json_object_array_length(coeffs_json);
            if (num_coeffs > FTMA_NUM_COEFFS)
            {
                log_error("Too many FTMA coeffs.");
                return false;
            }
            unsigned n;
            for (n = 0; n < num_coeffs; n++)
            {
                struct json_object * coeff_index = json_object_array_get_idx(coeffs_json, n);
                ftma->coeffs[n] = json_object_get_double(coeff_index);
            }
            for (; n < FTMA_NUM_COEFFS; n++)
                ftma->coeffs[n] = 0;
        }
    }
    return true;
}


static bool _read_json_to_img_base(struct json_object * root, persist_storage_t* config)
{
    if (!root || !config)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }
    struct json_object * obj = json_object_object_get(root, "version");
    if (!obj)
    {
        log_error("No version given.");
        return false;
    }

    config->version = json_object_get_int(obj);
    if (config->version > PERSIST_VERSION)
    {
        log_error("Wrong version.");
        return false;
    }
    log_out("JSON version = %"PRIu8, config->version);
    log_out("IMG  version = %"PRIu8, PERSIST_VERSION);
    if (config->version != PERSIST_VERSION)
        log_out("WARNING: Check compatibility!");

    config->model_code = _get_defaulted_int(root, "model_code", -1);

    config->log_debug_mask = _get_defaulted_int(root, "log_debug_mask", DEBUG_SYS);

    return true;
}


static bool _read_json_to_img_env01(struct json_object * root, persist_env01_config_v1_t * model_config)
{
    if (!root || !model_config)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }
    int tmp = _get_defaulted_int(root, "mins_interval",  15);
    if (tmp < 1 || tmp > (60 * 24))
    {
        log_error("Unsupported mins_interval");
        return false;
    }
    model_config->mins_interval  = tmp;

    comms_config_t* comms_config = &model_config->comms_config;
    switch(comms_config->type)
    {
        case COMMS_TYPE_LW:
            if (!_get_string_buf(root, "lw_dev_eui", ((lw_config_t*)(comms_config->setup))->dev_eui, LW_DEV_EUI_LEN) ||
                !_get_string_buf(root, "lw_app_key", ((lw_config_t*)(comms_config->setup))->app_key, LW_APP_KEY_LEN))
            {
                log_error("No LoRaWAN keys.");
                return false;
            }
            break;
    }

    env01_measurements_add_defaults(osm_mem.measurements->measurements_arr);

    if (!_read_modbus_json(root, &model_config->modbus_bus))
        return false;

    if (!_read_measurements_json(root))
        return false;

    if (!_read_ios_json(root, model_config->ios_state))
        return false;

    if (!_read_cc_configs_json(root, model_config->cc_configs))
        return false;

    return true;
}


static bool _read_json_to_img_sens01(struct json_object * root, persist_sens01_config_v1_t * model_config)
{
    if (!root || !model_config)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }

    int tmp = _get_defaulted_int(root, "mins_interval",  15);
    if (tmp < 1 || tmp > (60 * 24))
    {
        log_error("Unsupported mins_interval");
        return false;
    }
    model_config->mins_interval  = tmp;

    comms_config_t* comms_config = &model_config->comms_config;
    comms_config->type = _get_defaulted_int(root, "comms_type",  COMMS_TYPE_LW);
    switch(comms_config->type)
    {
        case COMMS_TYPE_LW:
            if (!_get_string_buf(root, "lw_dev_eui", ((lw_config_t*)(comms_config->setup))->dev_eui, LW_DEV_EUI_LEN) ||
                !_get_string_buf(root, "lw_app_key", ((lw_config_t*)(comms_config->setup))->app_key, LW_APP_KEY_LEN))
            {
                log_error("No LoRaWAN keys.");
                return false;
            }
            break;
    }

    sens01_measurements_add_defaults(osm_mem.measurements->measurements_arr);

    if (!_read_modbus_json(root, &model_config->modbus_bus))
        return false;

    if (!_read_measurements_json(root))
        return false;

    if (!_read_ios_json(root, model_config->ios_state))
        return false;

    if (!_read_ftma_configs_json(root, model_config->ftma_configs))
        return false;

    return true;
}


static bool _read_json_to_img_save(const char * filename)
{
    FILE * f = fopen(filename,"w");

    if (!f)
    {
        log_error("Failed to open file.");
        return false;
    }

    if (fwrite(osm_mem.config, osm_mem.config_size, 1, f) != 1)
    {
        log_error("Failed to write config.");
        fclose(f);
        return false;
    }

    if (fseek(f, TOOL_FLASH_PAGE_SIZE, SEEK_SET) != 0)
    {
        log_error("Failed to seek.");
        fclose(f);
        return false;
    }

    if (fwrite(osm_mem.measurements, osm_mem.measurements_size, 1, f) != 1)
    {
        log_error("Failed to write measurements.");
        fclose(f);
        return false;
    }
    fclose(f);

    return true;
}


int read_json_to_img(const char * filename)
{
    struct json_object * root = json_object_from_fd(0);
    if (!root)
    {
        log_error("Failed to read json : %s", json_util_get_last_err());
        goto bad_exit;
    }

    persist_storage_t base = {0};

    if (!_read_json_to_img_base(root, &base))
        goto bad_exit;

    bool r;
    switch (base.model_code)
    {
        case MODEL_NUM_ENV01:
        {
            osm_mem.config_size = sizeof(persist_storage_t) + sizeof(persist_env01_config_v1_t);

            osm_mem.config = calloc(osm_mem.config_size, 1);
            if (!osm_mem.config)
            {
                log_error("Failed to allocate memory for config");
                goto bad_exit;
            }

            osm_mem.measurements_size = sizeof(measurements_def_t) * MEASUREMENTS_MAX_NUMBER;
            osm_mem.measurements = calloc(osm_mem.measurements_size, 1);
            if (!osm_mem.measurements)
            {
                free(osm_mem.config);
                osm_mem.config = NULL;
                log_error("Failed to allocate memory for measurements");
                goto bad_exit;
            }

            memcpy(osm_mem.config, &base, sizeof(base));
            persist_env01_config_v1_t * model_config = (persist_env01_config_v1_t*)((uint8_t*)osm_mem.config) + sizeof(persist_storage_t);
            r = _read_json_to_img_env01(root, model_config);
            break;
        }
        case MODEL_NUM_SENS01:
        {
            osm_mem.config_size = sizeof(persist_storage_t) + sizeof(persist_sens01_config_v1_t);

            osm_mem.config = calloc(osm_mem.config_size, 1);
            if (!osm_mem.config)
            {
                log_error("Failed to allocate memory for config");
                goto bad_exit;
            }

            osm_mem.measurements_size = sizeof(measurements_def_t) * MEASUREMENTS_MAX_NUMBER;
            osm_mem.measurements = calloc(osm_mem.measurements_size, 1);
            if (!osm_mem.measurements)
            {
                free(osm_mem.config);
                osm_mem.config = NULL;
                log_error("Failed to allocate memory for measurements");
                goto bad_exit;
            }

            memcpy(osm_mem.config, &base, sizeof(base));
            persist_sens01_config_v1_t * model_config = (persist_sens01_config_v1_t*)&osm_mem.config[1];
            r = _read_json_to_img_sens01(root, model_config);
            break;
        }
        default:
            r = false;
            log_error("Unknown model code");
            break;
    }
    if (!r)
        goto bad_exit;

    if (!_read_json_to_img_save(filename))
        goto bad_exit;

    free(osm_mem.config);
    osm_mem.config = NULL;
    osm_mem.config_size = 0;
    free(osm_mem.measurements);
    osm_mem.measurements = NULL;
    osm_mem.measurements_size = 0;

    json_object_put(root);

    return EXIT_SUCCESS;

bad_exit:
    free(osm_mem.config);
    osm_mem.config = NULL;
    osm_mem.config_size = 0;
    free(osm_mem.measurements);
    osm_mem.measurements = NULL;
    osm_mem.measurements_size = 0;

    json_object_put(root);
    return EXIT_FAILURE;
}
