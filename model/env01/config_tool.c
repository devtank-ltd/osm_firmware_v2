#include "common_json.h"
#include "env01_config.h"
#include "env01.h"

static bool _write_json_from_img_env01(struct json_object * root, void * model_config_raw)
{
    persist_env01_config_v1_t* model_config = (persist_env01_config_v1_t*)model_config_raw;
    json_object_object_add(root, "mins_interval", json_object_new_int(model_config->mins_interval));

    comms_config_t* comms_config = &model_config->comms_config;
    if (!write_comms_json(root, comms_config))
        return false;

    write_cc_config_json(root, model_config->cc_configs, ADC_CC_COUNT);

    if (!write_modbus_json(root, &model_config->modbus_bus))
    {
        json_object_put(root);
        return false;
    }

    write_measurements_json(root);

    write_ios_json(root, model_config->ios_state);
    return true;
}


static bool _read_json_to_img_env01(struct json_object * root, void * model_config_raw)
{
    persist_env01_config_v1_t* model_config = (persist_env01_config_v1_t*)model_config_raw;
    if (!root || !model_config)
    {
        log_error("Handed a NULL pointer.");
        return false;
    }
    int tmp = get_defaulted_int(root, "mins_interval",  15);
    if (tmp < 1 || tmp > (60 * 24))
    {
        log_error("Unsupported mins_interval");
        return false;
    }
    model_config->mins_interval  = tmp;

    comms_config_t* comms_config = &model_config->comms_config;
    if (!read_comms_json(root, comms_config))
        return false;

    env01_measurements_add_defaults(osm_mem.measurements->measurements_arr);

    if (!read_modbus_json(root, &model_config->modbus_bus))
        return false;

    if (!read_measurements_json(root))
        return false;

    if (!read_ios_json(root, model_config->ios_state))
        return false;

    if (!read_cc_configs_json(root, model_config->cc_configs))
        return false;

    return true;
}


void __attribute__((constructor)) env01_register_config(void);


void env01_register_config(void)
{
    static struct model_config_funcs_t model_config_funcs = {ENV01_MODEL_NAME,
                                                             sizeof(persist_env01_config_v1_t),
                                                             _write_json_from_img_env01,
                                                             _read_json_to_img_env01,
                                                             NULL};

    model_config_funcs_register(&model_config_funcs);
}