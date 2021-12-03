#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/flash.h>

#include "log.h"
#include "persist_config.h"
#include "config.h"
#include "pinmap.h"
#include "timers.h"
#include "measurements.h"
#include "lorawan.h"

#define FLASH_ADDRESS               0x8000000
#define FLASH_SIZE                  (512 * 1024)
#define FLASH_PAGE_SIZE             2048
#define FLASH_CONFIG_PAGE           255
#define PERSIST__RAW_DATA           ((uint8_t*)(FLASH_ADDRESS + (FLASH_CONFIG_PAGE * FLASH_PAGE_SIZE)))
#define PERSIST__VERSION            1


typedef struct
{
    uint8_t     uuid;
    uint16_t    interval;
} __attribute__((__packed__)) persist_measurement_t;


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    char                    lw_dev_eui[LW__DEV_EUI_LEN];
    char                    lw_app_key[LW__APP_KEY_LEN];
    persist_measurement_t   p_measurements[LW__MAX_MEASUREMENTS];
    modbus_bus_config_t     modbus_bus_config;
} __attribute__((__packed__)) persist_storage_t;


static bool                 persist_data_valid = false;
static persist_storage_t    persist_data;


void init_persistent(void)
{
    persist_storage_t* persist_data_raw = (persist_storage_t*)PERSIST__RAW_DATA;

    uint32_t vs = persist_data_raw->version;
    if (vs != PERSIST__VERSION)
    {
        log_error("Persistent data version unknown.");
        return;
    }

    if (!persist_data_raw->lw_dev_eui[0] || !persist_data_raw->lw_app_key[0])
    {
        log_error("Persistent data empty.");
        return;
    }

    /*
    for(unsigned n = 0; n < LW__DEV_EUI_LEN; n++)
    {
        if (!isascii(persist_data_raw->lw_dev_eui[n]))
        {
            log_error("Persistent data not valid");
            return;
        }
    }
    for(unsigned n = 0; n < LW__APP_KEY_LEN; n++)
    {
        if (!isascii(persist_data_raw->lw_app_key[n]))
        {
            log_error("Persistent data not valid");
            return;
        }
    }
    */

    memcpy(&persist_data, persist_data_raw, sizeof(*persist_data_raw));
    persist_data_valid = true;
}


static void persistent_set_data(const void * addr, const void * data, unsigned size)
{
    uintptr_t _addr = (uintptr_t)addr;

    unsigned left_over = size % 8;
    unsigned easy_size = size - left_over;

    if (easy_size)
        flash_program(_addr, (void*)data, easy_size);

    if (size > easy_size)
    {
        const uint8_t * p = (const uint8_t*)data;

        uint64_t v = 0;

        for(unsigned n = 0; n < left_over; n+=1)
        {
             v |= (p[easy_size + n]) << (8*n);
        }

        flash_program_double_word(_addr + easy_size, v);
    }
}


static void _persistent_commit(void)
{
    flash_unlock();
    flash_erase_page(FLASH_CONFIG_PAGE);
    persistent_set_data(PERSIST__RAW_DATA, &persist_data, sizeof(persist_data));
    flash_lock();

    if (memcmp(PERSIST__RAW_DATA, &persist_data, sizeof(persist_data)) == 0)
        log_sys_debug("Flash successfully written.");
    else
        log_error("Flash write failed");
}


void persist_set_log_debug_mask(uint32_t mask)
{
    persist_data.version = PERSIST__VERSION;
    persist_data.log_debug_mask = mask;
    _persistent_commit();
}


uint32_t persist_get_log_debug_mask(void)
{
    if (!persist_data_valid)
    {
        log_out("Invalid data");
        return 0;
    }
    return persist_data.log_debug_mask;
}


void persist_set_lw_dev_eui(char* dev_eui)
{
    persist_data.version = PERSIST__VERSION;
    if (sizeof(dev_eui) > LW__DEV_EUI_LEN)
    {
        log_error("LORAWAN DEV EUI too long.");
        return;
    }
    memcpy(&persist_data.lw_dev_eui, &dev_eui, LW__DEV_EUI_LEN);
    _persistent_commit();
}


void persist_get_lw_dev_eui(char* dev_eui)
{
    if (sizeof(dev_eui) < LW__DEV_EUI_LEN + 1)
    {
        log_error("LORAWAN DEV EUI array too short");
        return;
    }
    memcpy(&dev_eui, &persist_data.lw_dev_eui, LW__DEV_EUI_LEN);
}


void persist_set_lw_app_key(char* app_key)
{
    persist_data.version = PERSIST__VERSION;
    if (sizeof(app_key) > LW__APP_KEY_LEN)
    {
        log_error("LORAWAN APP KEY too long.");
        return;
    }
    memcpy(&persist_data.lw_app_key, &app_key, LW__APP_KEY_LEN);
    _persistent_commit();
}


void persist_get_lw_app_key(char* app_key)
{
    if (sizeof(app_key) < LW__APP_KEY_LEN + 1)
    {
        log_error("LORAWAN APP KEY array too short");
        return;
    }
    memcpy(&app_key, &persist_data.lw_app_key, LW__APP_KEY_LEN);
}


bool persist_set_interval(uint8_t uuid, uint16_t interval)
{
    persist_data.version = PERSIST__VERSION;
    persist_measurement_t* measurement;
    uint16_t num_measurements = measurements_num_measurements();
    for (unsigned int i = 0; i < num_measurements; i++)
    {
        measurement = (persist_measurement_t*)&persist_data.p_measurements[i];
        if (measurement->uuid == uuid)
        {
            measurement->interval = interval;
            _persistent_commit();
            return true;
        }
    }
    return false;
}


bool persist_get_interval(uint8_t uuid, uint16_t* interval)
{
    persist_measurement_t* measurement;
    uint16_t num_measurements = measurements_num_measurements();
    for (unsigned int i = 0; i < num_measurements; i++)
    {
        measurement = (persist_measurement_t*)&persist_data.p_measurements[i];
        if (measurement->uuid == uuid)
        {
            *interval = measurement->interval;
            return true;
        }
    }
    return false;
}


void persist_new_interval(uint8_t uuid, uint16_t interval)
{
    persist_data.version = PERSIST__VERSION;
    persist_measurement_t* measurement;
    uint16_t num_measurements = measurements_num_measurements();
    if (persist_set_interval(uuid, interval))
    {
        return;
    }
    for (unsigned int i = 0; i < num_measurements; i++)
    {
        measurement = (persist_measurement_t*)&persist_data.p_measurements[i];
        if (measurement->uuid == 0)
        {
            measurement->uuid = uuid;
            measurement->interval = interval;
            _persistent_commit();
            return;
        }
    }
}


void persist_set_modbus_bus_config(modbus_bus_config_t* config)
{
    if (!config)
        return;
    persist_data.version = PERSIST__VERSION;
    memcpy(&persist_data.modbus_bus_config, config, sizeof(modbus_bus_config_t));
    _persistent_commit();
}


bool persist_get_modbus_bus_config(modbus_bus_config_t* config)
{
    if (!persist_data_valid || !config)
        return false;

    memcpy(config, &persist_data.modbus_bus_config, sizeof(modbus_bus_config_t));
    return true;
}
