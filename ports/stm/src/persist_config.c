#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <inttypes.h>

#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "platform.h"
#include "platform_model.h"
#include "common.h"
#include "io.h"

_Static_assert(sizeof(persist_storage_t) <= FLASH_PAGE_SIZE, "Persistent memory too large.");
_Static_assert(sizeof(persist_measurements_storage_t) <= FLASH_PAGE_SIZE, "Persistent measurements too large.");


persist_storage_t               persist_data __attribute__((aligned (16)));
persist_measurements_storage_t  persist_measurements __attribute__((aligned (16)));


bool persistent_init(void)
{
    persist_storage_t* persist_data_raw = platform_get_raw_persist();
    persist_measurements_storage_t* persist_measurements_raw = platform_get_measurements_raw_persist();

    bool wipe = false;

    if (!persist_data_raw ||
        !persist_measurements_raw)
    {
        log_error("Unable to load persistent data.");
        wipe = true;
    }
    else if (persist_data_raw->version != PERSIST_VERSION)
    {
        log_error("Persistent data version doesn't match.");
        wipe = true;
    }
    else if (strncmp(persist_data_raw->model_name, STR(fw_name), MODEL_NAME_LEN))
    {
        log_error("Persistent model name doesn't match.");
        wipe = true;
    }
    else if (persist_data_raw->model_config.comms_config.type != COMMS_BUILD_TYPE)
    {
        log_error("Persistent comms type doesn't match.");
        wipe = true;
    }

    if (wipe)
    {
        log_error("Setting persistent data.");
        memset(&persist_data, 0, sizeof(persist_data));
        memset(&persist_measurements, 0, sizeof(persist_measurements));
        persist_data.version = PERSIST_VERSION;
        persist_data.log_debug_mask = DEBUG_SYS;
        persist_data.config_count = 0;
        if (strlen(STR(fw_name)) > MODEL_NAME_LEN)
            memcpy(&persist_data.model_name[0], STR(fw_name), MODEL_NAME_LEN);
        else
            snprintf(persist_data.model_name, MODEL_NAME_LEN, STR(fw_name));
        unsigned human_name_len = snprintf(persist_data.human_name, HUMAN_NAME_LEN, "0x%08"PRIX32, platform_get_hw_id());
        persist_data.human_name[human_name_len] = 0;
        model_persist_config_model_init(&persist_data.model_config);
        return false;
    }

    memcpy(&persist_data, persist_data_raw, sizeof(persist_data));
    memcpy(&persist_measurements, persist_measurements_raw, sizeof(persist_measurements));
    return true;
}


/* Return true  if different
 *        false if same      */
static bool _persist_data_cmp(void)
{
    persist_storage_t* persist_data_raw = platform_get_raw_persist();
    return !(
        persist_data_raw                                                &&
        persist_data.log_debug_mask == persist_data_raw->log_debug_mask &&
        persist_data.version        == persist_data_raw->version        &&
        persist_data.pending_fw     == persist_data_raw->pending_fw     &&
        memcmp(persist_data.model_name,
            persist_data_raw->model_name,
            sizeof(char) * MODEL_NAME_LEN) == 0                         &&
        memcmp(persist_data.serial_number,
            persist_data_raw->serial_number,
            sizeof(char) * SERIAL_NUM_LEN_NULLED) == 0                  &&
        memcmp(persist_data.human_name,
            persist_data_raw->human_name,
            sizeof(char) * HUMAN_NAME_LEN_NULLED) == 0                  &&
        !model_persist_config_cmp(
            &persist_data.model_config,
            &persist_data_raw->model_config)                            &&
        persist_data.config_count   == persist_data_raw->config_count   );
}


/* Return true  if different
 *        false if same      */
static bool _persist_measurements_cmp(void)
{
    persist_measurements_storage_t* persist_measurements_raw = platform_get_measurements_raw_persist();
    return !(
        persist_measurements_raw                                        &&
        memcmp(&persist_measurements,
            persist_measurements_raw,
            sizeof(persist_measurements)) == 0                          );
}


void persist_commit()
{
    if (_persist_data_cmp()            ||
        _persist_measurements_cmp()    )
    {
        persist_data.config_count += 1;
        if (platform_persist_commit(&persist_data, &persist_measurements))
            log_sys_debug("Flash successfully written.");
        else
            log_error("Flash write failed");
    }
    else log_sys_debug("No changes to write to flash.");
}

void persist_set_fw_ready(uint32_t size)
{
    persist_data.pending_fw = size;
    persist_commit();
}
