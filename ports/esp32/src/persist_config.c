#include <string.h>

#include <nvs_flash.h>
#include <nvs.h>

#include "pinmap.h"
#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "platform.h"
#include "platform_model.h"


static nvs_handle_t config_handle;

persist_storage_t               persist_data __attribute__((aligned (16))) = {0};
persist_measurements_storage_t  persist_measurements __attribute__((aligned (16))) = {0};


static void _persistent_wipe(void)
{
    persist_data.version = PERSIST_VERSION;
    persist_data.log_debug_mask = DEBUG_SYS;
    persist_data.config_count = 0;
    if (strlen(MODEL_NAME) > MODEL_NAME_LEN)
        memcpy(&persist_data.model_name[0], MODEL_NAME, MODEL_NAME_LEN);
    else
        snprintf(persist_data.model_name, MODEL_NAME_LEN, MODEL_NAME);
    model_persist_config_model_init(&persist_data.model_config);
}


bool persistent_init(void)
{
    nvs_flash_init();
    ESP_ERROR_CHECK(nvs_open("osmconfig", NVS_READWRITE, &config_handle));
    size_t persist_data_read = 0, persist_measurements = 0;
    nvs_get_blob(config_handle, "persist_data", &persist_data, &persist_data_read);
    nvs_get_blob(config_handle, "persist_measurements", &persist_measurements, &persist_measurements);

    if (!persist_data_read || !persist_measurements)
        goto setup_exit;

    if (persist_data.version != PERSIST_VERSION)
        goto setup_exit;

    return true;
setup_exit:
    _persistent_wipe();
    return false;
}


char * persist_get_serial_number(void)
{
    return persist_data.serial_number;
}


void persist_set_log_debug_mask(uint32_t mask)
{
    persist_data.log_debug_mask = mask | DEBUG_SYS;
}


uint32_t persist_get_log_debug_mask(void)
{
    return persist_data.log_debug_mask;
}


void platform_persist_wipe(void)
{
    _persistent_wipe();
    persist_commit();
}


void     persist_commit()
{
    nvs_set_blob(config_handle, "persist_data", &persist_data, sizeof(persist_data));
    nvs_set_blob(config_handle, "persist_measurements", &persist_measurements, sizeof(persist_measurements));
}

