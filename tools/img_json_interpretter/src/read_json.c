#include "json_x_img.h"

#include "common_json.h"


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

    if (!get_string_buf(root, "model_name", config->model_name, MODEL_NAME_LEN))
    {
        log_error("Could not read model name.");
        return false;
    }

    config->log_debug_mask = get_defaulted_int(root, "log_debug_mask", DEBUG_SYS);

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

    struct model_config_funcs_t * model_config_funcs;

    if (!model_config_get(base.model_name, &model_config_funcs))
    {
        log_error("Unknown model for config \"%s\"", base.model_name);
        goto bad_exit;
    }

    osm_mem.config_size = sizeof(persist_storage_t) + model_config_funcs->model_config_size;

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
        log_error("Failed to allocate memory for measurements");
        goto bad_exit;
    }

    memcpy(osm_mem.config, &base, sizeof(base));

    if (!model_config_funcs->read_json_to_img_cb)
    {
        log_error("Model config does not have a read json callback for this model.");
        goto bad_exit;
    }

    if (!model_config_funcs->read_json_to_img_cb(root, (void*)&osm_mem.config->model_config))
    {
        log_error("Failed to read json for this model.");
        goto bad_exit;
    }

    if (!_read_json_to_img_save(filename))
    {
        log_error("Failed to write config to file.");
        goto bad_exit;
    }

    free_osm_mem();

    json_object_put(root);

    return EXIT_SUCCESS;

bad_exit:
    free_osm_mem();

    json_object_put(root);
    return EXIT_FAILURE;
}
