#include "json_x_img.h"

#include "persist_config.h"
#include "pinmap.h"
#include "modbus_mem.h"
#include "cc.h"
#include "ftma.h"


#define MAX_IO_COUNT                        20
#define WORD_BYTE_ORDER_TXT_SIZE            3


static struct model_config_funcs_t * _all_funcs = NULL;


void free_osm_mem(void)
{
    if (osm_mem.config)
    {
        free(osm_mem.config);
        osm_mem.config = NULL;
    }
    osm_mem.config_size = 0;

    if (osm_mem.measurements)
    {
        free(osm_mem.measurements);
        osm_mem.measurements = NULL;
    }
    osm_mem.measurements_size = 0;
}


bool model_config_get(char* model_name, struct model_config_funcs_t ** funcs)
{
    if (!model_name || !funcs)
        return false;
    for (struct model_config_funcs_t * f = _all_funcs; f; f = f->next)
    {
        size_t f_ptr_len = strlen(f->model_name);
        size_t funcs_len = strlen(model_name);
        if (f_ptr_len != funcs_len)
            continue;

        if (strncmp(model_name, f->model_name, f_ptr_len) == 0)
        {
            *funcs = f;
            return true;
        }
    }
    return false;
}


int get_defaulted_int(struct json_object * root, char * name, int default_value)
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return default_value;
    return json_object_get_int(tmp);
}


double get_defaulted_double(struct json_object * root, char * name, float default_value)
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return default_value;
    return json_object_get_double(tmp);
}


bool get_string_buf(struct json_object * root, char * name, char * buf, unsigned len)
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


void measurement_del(const char * name)
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


measurements_def_t* measurement_get_free(measurements_def_t * measurement_arr)
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


uint16_t get_io_pull(const char * str, uint16_t current)
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


uint16_t get_io_dir(const char * str, uint16_t current)
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


bool read_modbus_json(struct json_object * root, modbus_bus_t* modbus_bus)
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

        if (get_string_buf(modbus_bus_json, "con_str", con_str, sizeof(con_str)))
        {
            osm_uart_parity_t parity;
            osm_uart_stop_bits_t stopbits;
            uint8_t databits;;


            if (!osm_decompose_uart_str(con_str,
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
                if (!get_string_buf(dev_node, "word_order", w_b_order_txt, WORD_BYTE_ORDER_TXT_SIZE))
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
                if (!get_string_buf(dev_node, "byte_order", w_b_order_txt, WORD_BYTE_ORDER_TXT_SIZE))
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

                int unit_id = get_defaulted_int(dev_node, "unit_id", 0);
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
                        int               reg_addr = get_defaulted_int(reg_node, "reg", 0);
                        int               func     = get_defaulted_int(reg_node, "func", 0);

                        if (!modbus_dev_add_reg(dev, reg_name, type, (uint8_t)func, (uint16_t)reg_addr))
                            return false;

                        measurement_del(reg_name);

                        measurements_def_t * def = measurement_get_free(osm_mem.measurements->measurements_arr);
                        if (!def)
                            return false;
                        memcpy(def->name, reg_name, MODBUS_NAME_LEN);
                        def->name[MODBUS_NAME_LEN] = 0;
                        def->samplecount = 1;
                        def->interval    = 1;
                        def->type        = MODBUS;
                        def->is_immediate = 0;
                    }
                }
            }
        }
        log_out("Loaded modbus.");
        modbus_log();
    }
    return true;
}


bool read_measurements_json(struct json_object * root)
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

            int interval = get_defaulted_int(measurement_node, "interval", 1);
            if (interval < 0 || interval > 255)
            {
                log_error("Bad interval for measurement \"%s\"", measurement_name);
                return false;
            }
            int samplecount = get_defaulted_int(measurement_node, "samplecount", 1);
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


bool read_ios_json(struct json_object * root, uint16_t* ios_state, uint8_t io_count)
{
    if (!root || !ios_state)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }
    {
        uint16_t ios_init_state[MAX_IO_COUNT] = IOS_STATE;
        memcpy(ios_state, ios_init_state, sizeof(ios_state[0]) * io_count);
    }

    struct json_object * ios_node = json_object_object_get(root, "ios");
    if (ios_node)
    {
        json_object_object_foreach(ios_node, io_name, io_node)
        {
            unsigned index = strtoul(io_name, NULL, 10);
            if (index >= io_count)
            {
                log_error("Too many IOs");
                return false;
            }
            uint16_t state = ios_state[index];
            uint16_t pull    = get_io_pull(json_object_get_string(json_object_object_get(io_node, "pull")), state & IO_PULL_MASK);
            uint16_t dir     = get_io_dir(json_object_get_string(json_object_object_get(io_node, "direction")), state & IO_AS_INPUT);
            uint16_t w1_mode = (json_object_get_boolean(json_object_object_get(io_node, "use_w1")))?IO_SPECIAL_ONEWIRE:0;
            uint16_t pcnt_mode = (json_object_get_boolean(json_object_object_get(io_node, "use_pcnt")))?IO_SPECIAL_PULSECOUNT_RISING_EDGE:0;

            state &= ~(IO_PULL_MASK|IO_AS_INPUT);
            state &= ~IO_ACTIVE_SPECIAL_MASK;

            if (!dir)
                state |= (json_object_get_boolean(json_object_object_get(io_node, "out_high")))?IO_OUT_ON:0;

            ios_state[index] = state | (pull | dir | w1_mode | pcnt_mode);
        }
    }

    return true;
}


bool read_cc_configs_json(struct json_object * root, cc_config_t* cc_config)
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
            double midpointf = get_defaulted_double(cc_config_json, "midpoint", CC_DEFAULT_MIDPOINT / 1000.);
            midpointf *= 1000.;
            int64_t midpoint = midpointf;
            if (midpoint < 0 || midpoint > (int64_t)UINT32_MAX)
            {
                log_error("Bad midpoint for \"%s\"", cc_config_name);
                return false;
            }
            cc_config[index-1].midpoint = midpoint;
            int64_t ext_max_mA = get_defaulted_int(cc_config_json, "ext_max_mA", CC_DEFAULT_EXT_MAX_MA);
            if (ext_max_mA < 0 || ext_max_mA > (int64_t)UINT32_MAX)
            {
                log_error("Bad external maximum mA.");
                return false;
            }
            cc_config[index-1].ext_max_mA = ext_max_mA;
            int64_t int_max_mV = get_defaulted_int(cc_config_json, "int_max_mV", CC_DEFAULT_INT_MAX_MV);
            if (int_max_mV < 0 || ext_max_mA > (int64_t)UINT32_MAX)
            {
                log_error("Bad internal maximum mV.");
                return false;
            }
            cc_config[index-1].int_max_mV = int_max_mV;
        }
    }
    return true;
}


bool read_ftma_configs_json(struct json_object * root, ftma_config_t* ftma_configs)
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
            if (!get_string_buf(ftma_config_json, "name", ftma->name, MEASURE_NAME_LEN))
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
            int n;
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


bool read_comms_json(struct json_object * root, comms_config_t* comms_config)
{
    comms_config->type = get_defaulted_int(root, "comms_type",  COMMS_TYPE_LW);
    switch(comms_config->type)
    {
        case COMMS_TYPE_LW:
            if (!get_string_buf(root, "lw_dev_eui", ((lw_config_t*)(comms_config))->dev_eui, LW_DEV_EUI_LEN) ||
                !get_string_buf(root, "lw_app_key", ((lw_config_t*)(comms_config))->app_key, LW_APP_KEY_LEN))
            {
                log_error("No LoRaWAN keys.");
                return false;
            }
            break;
        default:
            log_error("Unknown comms num.");
            return false;
    }
    return true;
}


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
            strncpy(w_b_order_txt, "LSW", WORD_BYTE_ORDER_TXT_SIZE+1);
            break;
        case MODBUS_WORD_ORDER_MSW:
            strncpy(w_b_order_txt, "MSW", WORD_BYTE_ORDER_TXT_SIZE+1);
            break;
        default:
            strncpy(w_b_order_txt, "NA ", WORD_BYTE_ORDER_TXT_SIZE+1);
            break;
    }
    w_b_order_txt[WORD_BYTE_ORDER_TXT_SIZE] = 0;
    json_object_object_add(dev_node, "word_order", json_object_new_string(w_b_order_txt));
    switch(dev->byte_order)
    {
        case MODBUS_WORD_ORDER_LSW:
            strncpy(w_b_order_txt, "LSB", WORD_BYTE_ORDER_TXT_SIZE+1);
            break;
        case MODBUS_WORD_ORDER_MSW:
            strncpy(w_b_order_txt, "MSB", WORD_BYTE_ORDER_TXT_SIZE+1);
            break;
        default:
            strncpy(w_b_order_txt, "NA", WORD_BYTE_ORDER_TXT_SIZE+1);
            break;
    }
    w_b_order_txt[WORD_BYTE_ORDER_TXT_SIZE] = 0;
    json_object_object_add(dev_node, "byte_order", json_object_new_string(w_b_order_txt));

    json_object_object_add(dev_node, "unit_id", json_object_new_int(dev->unit_id));

    struct json_object * registers_node = json_object_new_object();

    json_object_object_add(dev_node, "registers", registers_node);

    return modbus_dev_for_each_reg(dev, _reg2json_cb, registers_node);
}


bool write_modbus_json(struct json_object * root, modbus_bus_t* modbus_bus)
{
    if (modbus_bus->version == MODBUS_BLOB_VERSION)
    {
        struct json_object * modbus_json = json_object_new_object();

        json_object_object_add(root, "modbus_bus", modbus_json);

        if (modbus_bus->binary_protocol)
            json_object_object_add(modbus_json, "binary_protocol", json_object_new_boolean(true));

        char con_str[32];

        snprintf(con_str, sizeof(con_str), "%"PRIu32" %u%c%s", modbus_bus->baudrate, modbus_bus->databits, osm_uart_parity_as_char(modbus_bus->parity), osm_uart_stop_bits_as_str(modbus_bus->stopbits));

        json_object_object_add(modbus_json, "con_str", json_object_new_string(con_str));

        struct json_object * modbus_devices_node = json_object_new_object();
        json_object_object_add(modbus_json, "modbus_devices", modbus_devices_node);

        modbus_for_each_dev(_dev2json_cb, modbus_devices_node);
    }
    return true;
}


void write_measurements_json(struct json_object * root)
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


void write_ios_json(struct json_object * root, uint16_t* ios_state, uint8_t io_count)
{
    struct json_object * ios_node = json_object_new_object();
    json_object_object_add(root, "ios", ios_node);

    for(unsigned n = 0; n < io_count; n++)
    {
        uint16_t state = ios_state[n];
        struct json_object * io_node = json_object_new_object();

        char name[8];
        snprintf(name, sizeof(name), "%u", n);

        json_object_object_add(ios_node, name, io_node);

        json_object_object_add(io_node, "pull", json_object_new_string(io_get_pull_str(state)));

        char * dir = (state & IO_AS_INPUT)?"IN":"OUT";

        json_object_object_add(io_node, "direction", json_object_new_string(dir));

        uint16_t special_state = state & IO_ACTIVE_SPECIAL_MASK;

        if (io_is_special(state) && (special_state == IO_SPECIAL_ONEWIRE))
            json_object_object_add(io_node, "use_w1", json_object_new_boolean(true));

        if (io_is_special(state) && (special_state == IO_SPECIAL_PULSECOUNT_RISING_EDGE     ||
                                     special_state == IO_SPECIAL_PULSECOUNT_FALLING_EDGE    ||
                                     special_state == IO_SPECIAL_PULSECOUNT_BOTH_EDGE       ))
            json_object_object_add(io_node, "use_pcnt", json_object_new_boolean(true));

        if (!(state & IO_AS_INPUT) && ((state & IO_STATE_MASK) == IO_OUT_ON))
            json_object_object_add(io_node, "out_high", json_object_new_boolean(true));
    }
}


void write_cc_config_json(struct json_object * root, cc_config_t* cc_configs, unsigned cc_count)
{
    struct json_object * cc_configs_json = json_object_new_object();
    json_object_object_add(root, "cc_configs", cc_configs_json);
    if (cc_count > 8)
    {
        log_error("CC count beyond supported.");
        return;
    }
    char name[4];
    for (unsigned n = 0; n < cc_count; n++)
    {
        snprintf(name, 4, "CC%u", n+1);
        struct json_object * cc_config_json = json_object_new_object();
        json_object_object_add(cc_configs_json, name, cc_config_json);

        json_object_object_add(cc_config_json, "midpoint", json_object_new_double((double)cc_configs[n].midpoint / 1000.));
        json_object_object_add(cc_config_json, "ext_max_mA", json_object_new_int(cc_configs[n].ext_max_mA));
        json_object_object_add(cc_config_json, "int_max_mV", json_object_new_int(cc_configs[n].int_max_mV));
    }
}


void write_ftma_config_json(struct json_object * root, ftma_config_t* ftma_configs, unsigned ftma_count)
{
    struct json_object * ftma_configs_json = json_object_new_object();
    json_object_object_add(root, "ftma_configs", ftma_configs_json);
    if (ftma_count > 8)
    {
        log_error("FTMA count beyond supported.");
        return;
    }
    char name[MEASURE_NAME_NULLED_LEN];
    for (unsigned n = 0; n < ftma_count; n++)
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


bool write_comms_json(struct json_object * root, comms_config_t* comms_config)
{
    json_object_object_add(root, "comms_type", json_object_new_int(comms_config->type));
    switch(comms_config->type)
    {
        case COMMS_TYPE_LW:
        {
            char* dev_eui = ((lw_config_t*)comms_config)->dev_eui;
            char* app_key = ((lw_config_t*)comms_config)->app_key;
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
    return true;
}


void model_config_funcs_register(struct model_config_funcs_t * funcs)
{
    if (_all_funcs)
        funcs->next = _all_funcs;
    _all_funcs = funcs;
}
