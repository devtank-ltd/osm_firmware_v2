#include "json_x_img.h"

#include "common_json.h"


static bool _write_json_from_img_base(struct json_object * root, persist_storage_t* config)
{
    json_object_object_add(root, "version", json_object_new_int(PERSIST_VERSION));

    json_object_object_add(root, "log_debug_mask", json_object_new_int(config->log_debug_mask));

    json_object_object_add(root, "model_name", json_object_new_string_len(config->model_name, strnlen(config->model_name, MODEL_NAME_LEN)));
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

    struct model_config_funcs_t * model_config_funcs;

    if (!model_config_get(base.model_name, &model_config_funcs))
    {
        log_error("Unknown model for config \"%s\"", base.model_name);
        goto bad_exit;
    }

    osm_mem.config_size = sizeof(persist_storage_t) + model_config_funcs->model_config_size;

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
    void * model_config = ((uint8_t*)osm_mem.config) + sizeof(persist_storage_t);

    if (fread(model_config, model_config_funcs->model_config_size, 1, f) != 1)
    {
        log_error("Failed to read config.");
        goto bad_exit;
    }

    if (fseek(f, TOOL_FLASH_PAGE_SIZE, SEEK_SET) != 0)
    {
        log_error("Failed to seek.");
        goto bad_exit;
    }

    if (fread(osm_mem.measurements, osm_mem.measurements_size, 1, f) != 1)
    {
        log_error("Failed to read config.");
        goto bad_exit;
    }

    if (!model_config_funcs->write_json_from_img_cb)
    {
        log_error("Model config has no write function.");
        goto bad_exit;
    }

    if (!model_config_funcs->write_json_from_img_cb(root, (void*)model_config))
    {
        log_error("Failed to write json for this model.");
        goto bad_exit;
    }

    free_osm_mem();

    json_object_to_fd(1, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    fclose(f);

    return EXIT_SUCCESS;

bad_exit:
    free_osm_mem();

    fclose(f);
    json_object_put(root);
    return EXIT_FAILURE;
}
