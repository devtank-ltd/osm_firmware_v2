#include "json_x_img.h"

#define WORD_BYTE_ORDER_TXT_SIZE            3

static bool _reg2json_cb(modbus_reg_t * reg, void * userdata)
{
    struct json_object * registers_node=(struct json_object*)userdata;
    struct json_object * register_node = json_object_new_object();

    char name[MODBUS_NAME_LEN+1];

    memcpy(name, reg->name, MODBUS_NAME_LEN);
    name[MODBUS_NAME_LEN] = 0;

    json_object_object_add(registers_node, name, register_node);

    const char * type = modbus_reg_type_get_str(reg->type);

    if (!type)
    {
        log_error("Config has invalid register type for \"%s\"", name);
        return true;
    }

    json_object_object_add(register_node, "type", json_object_new_string(type));
    json_object_object_add(register_node, "reg", json_object_new_int(reg->reg_addr));
    json_object_object_add(register_node, "func", json_object_new_int(reg->func));

    return false;
}


static bool _dev2json_cb(modbus_dev_t * dev, void * userdata)
{
    struct json_object * modbus_devices_node=(struct json_object*)userdata;
    struct json_object * dev_node = json_object_new_object();

    char name[MODBUS_NAME_LEN+1];

    memcpy(name, dev->name, MODBUS_NAME_LEN);
    name[MODBUS_NAME_LEN] = 0;

    json_object_object_add(modbus_devices_node, name, dev_node);

    char w_b_order_txt[WORD_BYTE_ORDER_TXT_SIZE + 1];
    switch(dev->word_order)
    {
        case MODBUS_WORD_ORDER_LSW:
            strncpy(w_b_order_txt, "LSW", WORD_BYTE_ORDER_TXT_SIZE);
            break;
        case MODBUS_WORD_ORDER_MSW:
            strncpy(w_b_order_txt, "MSW", WORD_BYTE_ORDER_TXT_SIZE);
            break;
        default:
            strncpy(w_b_order_txt, "NA ", WORD_BYTE_ORDER_TXT_SIZE);
            break;
    }
    w_b_order_txt[WORD_BYTE_ORDER_TXT_SIZE] = 0;
    json_object_object_add(dev_node, "word_order", json_object_new_string(w_b_order_txt));
    switch(dev->byte_order)
    {
        case MODBUS_WORD_ORDER_LSW:
            strncpy(w_b_order_txt, "LSB", WORD_BYTE_ORDER_TXT_SIZE);
            break;
        case MODBUS_WORD_ORDER_MSW:
            strncpy(w_b_order_txt, "MSB", WORD_BYTE_ORDER_TXT_SIZE);
            break;
        default:
            strncpy(w_b_order_txt, "NA", WORD_BYTE_ORDER_TXT_SIZE);
            break;
    }
    w_b_order_txt[WORD_BYTE_ORDER_TXT_SIZE] = 0;
    json_object_object_add(dev_node, "byte_order", json_object_new_string(w_b_order_txt));

    json_object_object_add(dev_node, "unit_id", json_object_new_int(dev->unit_id));

    struct json_object * registers_node = json_object_new_object();

    json_object_object_add(dev_node, "registers", registers_node);

    return modbus_dev_for_each_reg(dev, _reg2json_cb, registers_node);
}


static bool _write_modbus_json(struct json_object * root, modbus_bus_t* modbus_bus)
{
    if (modbus_bus->version == MODBUS_BLOB_VERSION)
    {
        struct json_object * modbus_json = json_object_new_object();

        json_object_object_add(root, "modbus_bus", modbus_json);

        if (modbus_bus->binary_protocol)
            json_object_object_add(modbus_json, "binary_protocol", json_object_new_boolean(true));

        char con_str[32];

        snprintf(con_str, sizeof(con_str), "%"PRIu32" %u%c%s", modbus_bus->baudrate, modbus_bus->databits, uart_parity_as_char(modbus_bus->parity), uart_stop_bits_as_str(modbus_bus->stopbits));

        json_object_object_add(modbus_json, "con_str", json_object_new_string(con_str));

        struct json_object * modbus_devices_node = json_object_new_object();
        json_object_object_add(modbus_json, "modbus_devices", modbus_devices_node);

        modbus_for_each_dev(_dev2json_cb, modbus_devices_node);
    }
    return true;
}


static void _write_measurements_json(struct json_object * root)
{
    struct json_object * measurements_node = json_object_new_object();

    json_object_object_add(root, "measurements", measurements_node);

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t * def = &osm_mem.measurements->measurements_arr[i];
        if (def->name[0])
        {
            struct json_object * measurement_node = json_object_new_object();

            json_object_object_add(measurements_node, def->name, measurement_node);
            json_object_object_add(measurement_node, "interval", json_object_new_int(def->interval));
            json_object_object_add(measurement_node, "samplecount", json_object_new_int(def->samplecount));
        }
    }
}


static void _write_ios_json(struct json_object * root, uint16_t* ios_state)
{
    struct json_object * ios_node = json_object_new_object();
    json_object_object_add(root, "ios", ios_node);

    for(unsigned n = 0; n < IOS_COUNT; n++)
    {
        uint16_t state = ios_state[n];
        struct json_object * io_node = json_object_new_object();

        char name[8];
        snprintf(name, sizeof(name), "%u", n);

        json_object_object_add(ios_node, name, io_node);

        json_object_object_add(io_node, "pull", json_object_new_string(io_get_pull_str(state)));

        char * dir = (state & IO_AS_INPUT)?"IN":"OUT";

        json_object_object_add(io_node, "direction", json_object_new_string(dir));

        if (io_is_special(state) && ((state & IO_STATE_MASK) == IO_ONEWIRE))
            json_object_object_add(io_node, "use_w1", json_object_new_boolean(true));

        if (io_is_special(state) && ((state & IO_STATE_MASK) == IO_PULSE))
            json_object_object_add(io_node, "use_pcnt", json_object_new_boolean(true));

        if (!(state & IO_AS_INPUT) && ((state & IO_STATE_MASK) == IO_OUT_ON))
            json_object_object_add(io_node, "out_high", json_object_new_boolean(true));
    }
}


static void _write_cc_config_json(struct json_object * root, cc_config_t* cc_configs)
{
    struct json_object * cc_configs_json = json_object_new_object();
    json_object_object_add(root, "cc_configs", cc_configs_json);
    char name[4];
    for (unsigned n = 0; n < ADC_CC_COUNT; n++)
    {
        snprintf(name, 4, "CC%u", n+1);
        struct json_object * cc_config_json = json_object_new_object();
        json_object_object_add(cc_configs_json, name, cc_config_json);

        json_object_object_add(cc_config_json, "midpoint", json_object_new_double((double)cc_configs[n].midpoint / 1000.));
        json_object_object_add(cc_config_json, "ext_max_mA", json_object_new_int(cc_configs[n].ext_max_mA));
        json_object_object_add(cc_config_json, "int_max_mV", json_object_new_int(cc_configs[n].int_max_mV));
    }
}


static bool _write_ftma_config_json(struct json_object * root, ftma_config_t* ftma_configs)
{
    struct json_object * ftma_configs_json = json_object_new_object();
    json_object_object_add(root, "ftma_configs", ftma_configs_json);
    char name[MEASURE_NAME_NULLED_LEN];
    for (unsigned n = 0; n < ADC_FTMA_COUNT; n++)
    {
        snprintf(name, MEASURE_NAME_NULLED_LEN, "FTA%u", n+1);
        struct json_object * ftma_config_json = json_object_new_object();
        json_object_object_add(ftma_configs_json, name, ftma_config_json);

        unsigned name_len = strnlen(ftma_configs[n].name, MEASURE_NAME_LEN);
        json_object_object_add(ftma_config_json, "name", json_object_new_string_len(ftma_configs[n].name, name_len));
        struct json_object * coeff_array_json = json_object_new_array();
        json_object_object_add(ftma_config_json, "coeffs", coeff_array_json);
        for (unsigned m = 0; m < FTMA_NUM_COEFFS; m++)
            json_object_array_add(coeff_array_json, json_object_new_double(ftma_configs[n].coeffs[m]));
    }
}


static bool _write_json_from_img_base(struct json_object * root, persist_storage_t* config)
{
    json_object_object_add(root, "version", json_object_new_int(PERSIST_VERSION));

    json_object_object_add(root, "log_debug_mask", json_object_new_int(config->log_debug_mask));

    json_object_object_add(root, "model_code", json_object_new_int(config->model_code));
    return true;
}


static bool _write_json_from_img_env01(struct json_object * root, persist_env01_config_v1_t* model_config)
{
    json_object_object_add(root, "mins_interval", json_object_new_int(model_config->mins_interval));

    comms_config_t* comms_config = &model_config->comms_config;
    json_object_object_add(root, "comms_type", json_object_new_int(comms_config->type));
    switch(comms_config->type)
    {
        case COMMS_TYPE_LW:
        {
            char* dev_eui = ((lw_config_t*)comms_config->setup)->dev_eui;
            char* app_key = ((lw_config_t*)comms_config->setup)->app_key;
            unsigned dev_eui_len = strnlen(dev_eui, LW_DEV_EUI_LEN);
            unsigned app_key_len = strnlen(app_key, LW_APP_KEY_LEN);
            json_object_object_add(root, "lw_dev_eui", json_object_new_string_len(dev_eui, dev_eui_len));
            json_object_object_add(root, "lw_app_key", json_object_new_string_len(app_key, app_key_len));
            break;
        }
        default:
        {
            log_error("Unknown comms type.");
            return false;
        }
    }


    _write_cc_config_json(root, model_config->cc_configs);

    if (!_write_modbus_json(root, &model_config->modbus_bus))
    {
        json_object_put(root);
        return false;
    }

    _write_measurements_json(root);

    _write_ios_json(root, model_config->ios_state);
    return true;
}


static bool _write_json_from_img_sens01(struct json_object * root, persist_sens01_config_v1_t* model_config)
{
    json_object_object_add(root, "mins_interval", json_object_new_int(model_config->mins_interval));

    comms_config_t* comms_config = &model_config->comms_config;
    json_object_object_add(root, "comms_type", json_object_new_int(comms_config->type));
    switch(comms_config->type)
    {
        case COMMS_TYPE_LW:
        {
            char* dev_eui = ((lw_config_t*)comms_config->setup)->dev_eui;
            char* app_key = ((lw_config_t*)comms_config->setup)->app_key;
            unsigned dev_eui_len = strnlen(dev_eui, LW_DEV_EUI_LEN);
            unsigned app_key_len = strnlen(app_key, LW_APP_KEY_LEN);
            json_object_object_add(root, "lw_dev_eui", json_object_new_string_len(dev_eui, dev_eui_len));
            json_object_object_add(root, "lw_app_key", json_object_new_string_len(app_key, app_key_len));
            break;
        }
        default:
        {
            log_error("Unknown comms type.");
            return false;
        }
    }


    _write_ftma_config_json(root, model_config->ftma_configs);

    if (!_write_modbus_json(root, &model_config->modbus_bus))
    {
        json_object_put(root);
        return false;
    }

    _write_measurements_json(root);

    _write_ios_json(root, model_config->ios_state);
    return true;
}


int write_json_from_img(const char * filename)
{
    FILE * f = fopen(filename,"r");

    if (!f)
    {
        log_error("Failed to open file.");
        return EXIT_FAILURE;
    }

    persist_storage_t base = {0};

    if (fread(&base, sizeof(base), 1, f) != 1)
    {
        log_error("Failed to read base.");
        fclose(f);
        return EXIT_FAILURE;
    }

    struct json_object * root = json_object_new_object();

    if (!_write_json_from_img_base(root, &base))
        goto bad_exit;

    bool r;
    switch (base.model_code)
    {
        case MODEL_NUM_ENV01:
        {
            osm_mem.config_size = sizeof(persist_storage_t) + sizeof(persist_env01_config_v1_t);

            osm_mem.config = malloc(osm_mem.config_size);
            if (!osm_mem.config)
            {
                log_error("Failed to allocate memory for config.");
                goto bad_exit;
            }
            osm_mem.measurements_size = sizeof(measurements_def_t) * MEASUREMENTS_MAX_NUMBER;
            osm_mem.measurements = malloc(osm_mem.measurements_size);
            if (!osm_mem.measurements)
            {
                log_error("Failed to allocate memory for measurements.");
                goto bad_exit;
            }
            memcpy(osm_mem.config, &base, sizeof(base));
            persist_env01_config_v1_t * model_config = (persist_env01_config_v1_t*)((uint8_t*)osm_mem.config) + sizeof(persist_storage_t);

            if (fread(model_config, sizeof(persist_env01_config_v1_t), 1, f) != 1)
            {
                log_error("Failed to read config.");
                r = false;
                goto env01_exit;
            }

            if (fseek(f, TOOL_FLASH_PAGE_SIZE, SEEK_SET) != 0)
            {
                log_error("Failed to seek.");
                r = false;
                goto env01_exit;
            }

            if (fread(osm_mem.measurements, osm_mem.measurements_size, 1, f) != 1)
            {
                log_error("Failed to read measurements.");
                r = false;
                goto env01_exit;
            }

            r = _write_json_from_img_env01(root, model_config);

env01_exit:
            free(osm_mem.config);
            osm_mem.config = NULL;
            free(osm_mem.measurements);
            osm_mem.measurements = NULL;
            break;
        }
        case MODEL_NUM_SENS01:
        {
            osm_mem.config_size = sizeof(persist_storage_t) + sizeof(persist_sens01_config_v1_t);

            osm_mem.config = malloc(osm_mem.config_size);
            if (!osm_mem.config)
            {
                log_error("Failed to allocate memory for config.");
                goto bad_exit;
            }
            osm_mem.measurements_size = sizeof(measurements_def_t) * MEASUREMENTS_MAX_NUMBER;
            osm_mem.measurements = malloc(osm_mem.measurements_size);
            if (!osm_mem.measurements)
            {
                free(osm_mem.config);
                osm_mem.config = NULL;
                log_error("Failed to allocate memory for measurements.");
                goto bad_exit;
            }
            memcpy(osm_mem.config, &base, sizeof(base));
            persist_sens01_config_v1_t * model_config = (persist_sens01_config_v1_t*)&osm_mem.config[1];

            if (fread(model_config, sizeof(persist_sens01_config_v1_t), 1, f) != 1)
            {
                log_error("Failed to read config.");
                r = false;
                goto sens01_exit;
            }

            if (fseek(f, TOOL_FLASH_PAGE_SIZE, SEEK_SET) != 0)
            {
                log_error("Failed to seek.");
                r = false;
                goto sens01_exit;
            }

            if (fread(osm_mem.measurements, osm_mem.measurements_size, 1, f) != 1)
            {
                log_error("Failed to read measurements.");
                r = false;
                goto sens01_exit;
            }

            r = _write_json_from_img_sens01(root, model_config);

sens01_exit:
            free(osm_mem.config);
            osm_mem.config = NULL;
            free(osm_mem.measurements);
            osm_mem.measurements = NULL;
            break;
        }
        default:
            r = false;
            log_error("Unknown model code");
            break;
    }
    if (!r)
        goto bad_exit;

    json_object_to_fd(1, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    fclose(f);

    return EXIT_SUCCESS;

bad_exit:
    fclose(f);
    json_object_put(root);
    return EXIT_FAILURE;
}
