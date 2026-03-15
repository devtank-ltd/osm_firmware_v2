#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <inttypes.h>

#include "hardware/flash.h"

#include <osm/core/config.h>
#include <osm/core/log.h>
#include <osm/core/persist_config.h>
#include <osm/core/persist_config_header.h>
#include <osm/core/platform.h>
#include "platform_model.h"
#include <osm/core/common.h>

_Static_assert(sizeof(osm_persist_storage_t) <= FLASH_PAGE_SIZE * 8, "Persistent memory too large.");
_Static_assert(sizeof(osm_persist_measurements_storage_t) <= FLASH_PAGE_SIZE * 8, "Persistent measurements too large.");


osm_persist_storage_t               persist_data __attribute__((aligned (16)));
osm_persist_measurements_storage_t  persist_measurements __attribute__((aligned (16)));


bool osm_persistent_init(void)
{
    osm_persist_storage_t* persist_data_raw = osm_platform_get_raw_persist();
    osm_persist_measurements_storage_t* persist_measurements_raw = osm_platform_get_measurements_raw_persist();

    bool wipe = false;

    if (!persist_data_raw ||
        !persist_measurements_raw)
    {
        osm_log_error("Unable to load persistent data.");
        wipe = true;
    }
    else if (persist_data_raw->version != PERSIST_VERSION)
    {
        osm_log_error("Persistent data version doesn't match.");
        wipe = true;
    }
    else if (strncmp(persist_data_raw->model_name, STR(fw_name), OSM_MODEL_NAME_LEN))
    {
        osm_log_error("Persistent model name doesn't match.");
        wipe = true;
    }

    if (wipe)
    {
        osm_log_error("Setting persistent data.");
        memset(&persist_data, 0, sizeof(persist_data));
        memset(&persist_measurements, 0, sizeof(persist_measurements));
        persist_data.version = PERSIST_VERSION;
        persist_data.log_debug_mask = OSM_DEBUG_SYS;
        persist_data.config_count = 0;
        if (strlen(STR(fw_name)) > OSM_MODEL_NAME_LEN)
            memcpy(&persist_data.model_name[0], STR(fw_name), OSM_MODEL_NAME_LEN);
        else
            snprintf(persist_data.model_name, OSM_MODEL_NAME_LEN, STR(fw_name));
        unsigned human_name_len = snprintf(persist_data.human_name, OSM_HUMAN_NAME_LEN, "0x%08"PRIX32, osm_platform_get_hw_id());
        persist_data.human_name[human_name_len] = 0;
        /* Copy model_config out to avoid unaligned pointers due to using
         * a packed member
         */
        osm_persist_model_config_t model_config = persist_data.model_config;
        osm_model_persist_config_model_init(&model_config);
        persist_data.model_config = model_config;
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
    osm_persist_storage_t* persist_data_raw = osm_platform_get_raw_persist();
    /* Copy model_config out to avoid unaligned pointers due to using
     * a packed member
     */
    osm_persist_model_config_t model_config = persist_data.model_config;
    osm_persist_model_config_t model_config_raw = persist_data_raw->model_config;
    return !(
        persist_data_raw                                                &&
        persist_data.log_debug_mask == persist_data_raw->log_debug_mask &&
        persist_data.version        == persist_data_raw->version        &&
        persist_data.pending_fw     == persist_data_raw->pending_fw     &&
        memcmp(persist_data.model_name,
            persist_data_raw->model_name,
            sizeof(char) * OSM_MODEL_NAME_LEN) == 0                         &&
        memcmp(persist_data.serial_number,
            persist_data_raw->serial_number,
            sizeof(char) * OSM_SERIAL_NUM_LEN_NULLED) == 0              &&
        memcmp(persist_data.human_name,
            persist_data_raw->human_name,
            sizeof(char) * OSM_HUMAN_NAME_LEN_NULLED) == 0              &&
        !osm_model_persist_config_cmp(
            &model_config,
            &model_config_raw)                                          &&
        persist_data.config_count   == persist_data_raw->config_count   );
}


/* Return true  if different
 *        false if same      */
static bool _persist_measurements_cmp(void)
{
    osm_persist_measurements_storage_t* persist_measurements_raw = osm_platform_get_measurements_raw_persist();
    return !(
        persist_measurements_raw                                        &&
        memcmp(&persist_measurements,
            persist_measurements_raw,
            sizeof(persist_measurements)) == 0                          );
}


void osm_persist_commit()
{
    if (_persist_data_cmp()            ||
        _persist_measurements_cmp()    )
    {
        persist_data.config_count += 1;
        if (osm_platform_persist_commit(&persist_data, &persist_measurements))
            osm_log_sys_debug("Flash successfully written.");
        else
            osm_log_error("Flash write failed");
    }
    else osm_log_sys_debug("No changes to write to flash.");
}

void osm_persist_set_fw_ready(uint32_t size)
{
    persist_data.pending_fw = size;
    osm_persist_commit();
}
