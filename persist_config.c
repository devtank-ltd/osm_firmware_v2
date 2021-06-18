#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/flash.h>

#include "log.h"
#include "persist_config.h"
#include "config.h"
#include "pinmap.h"
#include "timers.h"

#define FLASH_ADDRESS 0x8000000
#define FLASH_PAGE_SIZE 2048
#define RAW_PERSIST_DATA ((void*)0x801F800)
#define RAW_PERSIST_SIZE 2048
#define PERSIST_VERSION 3

#define CONFIG_NAME_MAX_LEN 32

typedef struct
{
    union
    {
        struct
        {
            uint64_t offset;
            uint64_t scale;
        };
        uint64_t cals[MAX_ADC_CAL_POLY_NUM];
    };
    uint8_t poly_len;
} __attribute__((packed)) cal_data_t;

typedef char cal_unit_t[4];

typedef union
{
    struct
    {
        uint32_t  use_adc_cals:1;
    };
    uint32_t raw;
} __attribute__((packed)) config_flags_t;


typedef struct
{
    uint32_t       version;
    config_flags_t flags;
    uint32_t       _;
    uint32_t       adc_sample_rate;
    char           config_name[CONFIG_NAME_MAX_LEN];
    cal_data_t     cals[ADC_COUNT];
    cal_unit_t     units[ADC_COUNT];
} __attribute__((packed)) config_data_t;


static bool          config_data_valid = false;
static config_data_t config_data;




void        init_persistent()
{
    config_data_t * config_data_raw = (config_data_t*)RAW_PERSIST_DATA;

    if (config_data_raw->version != PERSIST_VERSION)
    {
        log_error("Persistent data version unknown.");
        return;
    }

    if (!config_data_raw->config_name[0])
    {
        log_error("Persistent data empty.");
        return;
    }

    for(unsigned n = 0; n < 32; n++)
    {
        if (!isascii(config_data_raw->config_name[n]))
        {
            log_error("Persistent data not valid");
            return;
        }

        if (!config_data_raw->config_name[n])
        {
            log_sys_debug("Found named persistent data : %s", config_data_raw->config_name);
            break;
        }
    }

    memcpy(&config_data, config_data_raw, sizeof(config_data));
    if (!timer_set_adc_boardary(config_data.adc_sample_rate))
        log_error("Failed to set ADC sample rate from config.");
    config_data_valid = true;
}


const char* persistent_get_name()
{
    if (config_data_valid)
        return config_data.config_name;
    return NULL;
}


static void persistent_set_data(const void * addr, const void * data, unsigned size)
{
    const uint8_t * p = (const uint8_t*)data;
    uintptr_t _addr = (uintptr_t)addr;

    for(unsigned n = 0; n < size; n+=2)
    {
        uint16_t v;
        if ((n + 1) == size) /* Make sure we don't copy too much.*/
            v = p[n];
        else
            v = p[n] | (p[n + 1] << 8);

        flash_program_half_word(_addr + n, v);
    }
}


static void _persistent_commit()
{
    flash_unlock();

    flash_erase_page((uintptr_t)RAW_PERSIST_DATA);

    config_data_valid = true;

    persistent_set_data(RAW_PERSIST_DATA, &config_data, sizeof(config_data));

    flash_lock();
}


void        persistent_set_name(const char * name)
{
    unsigned len = strlen(name) + 1;

    if (len > CONFIG_NAME_MAX_LEN)
    {
        log_error("Persistent name too long.");
        len = CONFIG_NAME_MAX_LEN;
    }

    config_data.version = PERSIST_VERSION;
    config_data.flags.raw = 0;
    config_data.flags.use_adc_cals = 1;

    for(unsigned n = 0; n < CONFIG_NAME_MAX_LEN; n++)
    {
        if (n < len)
            config_data.config_name[n] = name[n];
        else
            config_data.config_name[n] = 0;
    }

    /* Default calibration values are just 3.3v */
    basic_fixed_t temp;
    basic_fixed_t v3_3;

    basic_fixed_set_whole(&v3_3, 3300);
    basic_fixed_set_whole(&temp, 4095);
    basic_fixed_div(&v3_3, &v3_3, &temp); /* 3.3v / 4095 */

    basic_fixed_set_whole(&temp, 0);

    for(unsigned n = 0; n < ADC_COUNT; n++)
    {
        cal_data_t * cal = &config_data.cals[n];
        cal->scale = v3_3.raw;
        cal->offset = temp.raw;
        memcpy(&config_data.units[n], "mV", 3);
    }

    config_data.adc_sample_rate = timer_get_adc_boardary();

    _persistent_commit();
}


bool        persistent_get_cal_type(unsigned adc, cal_type_t * type)
{
    if (adc >= ADC_COUNT || !type)
        return false;

    if (!config_data_valid)
    {
        *type = ADC_NO_CAL;
        return true;
    }

    cal_data_t * cal = &config_data.cals[adc];

    *type = (cal->poly_len)?ADC_POLY_CAL:ADC_LIN_CAL;
    return true;
}


bool        persistent_get_cal(unsigned adc, basic_fixed_t * scale, basic_fixed_t * offset, const char ** unit)
{
    if (adc >= ADC_COUNT || !config_data_valid ||  !offset || !scale || !unit)
        return false;

    cal_data_t * cal = &config_data.cals[adc];

    if (cal->poly_len)
        return false;

    offset->raw = cal->offset;
    scale->raw = cal->scale;
    *unit = config_data.units[adc];
    return true;
}


bool        persistent_set_cal(unsigned adc, basic_fixed_t * scale, basic_fixed_t * offset, const char * unit)
{
    if (adc >= ADC_COUNT || !config_data_valid)
        return false;

    cal_data_t * cal = &config_data.cals[adc];

    if (unit)
    {
        unsigned len = strlen(unit) + 1;

        memcpy(&config_data.units[adc], unit, (len < sizeof(cal_unit_t))?len:sizeof(cal_unit_t));
        config_data.units[adc][sizeof(cal_unit_t)-1] = 0;
    }

    if (scale)
        cal->scale = scale->raw;

    if (offset)
        cal->offset = offset->raw;

    cal->poly_len = 0;

    _persistent_commit();

   return true;
}


bool        persistent_get_exp_cal(unsigned adc, basic_fixed_t ** cals, unsigned * count, const char ** unit)
{
    if (adc >= ADC_COUNT || !config_data_valid ||  !cals || !count || !unit)
        return false;

    cal_data_t * cal = &config_data.cals[adc];

    if (!cal->poly_len)
        return false;

    *cals = (basic_fixed_t*)cal->cals;

    *count = cal->poly_len;
    *unit = config_data.units[adc];
    return true;
}


bool        persistent_set_exp_cal(unsigned adc, basic_fixed_t * cals, unsigned count, const char * unit)
{
    if (adc >= ADC_COUNT || !config_data_valid)
        return false;

    if (count > MAX_ADC_CAL_POLY_NUM)
    {
        log_error("Polynomial count over limit.");
        return false;
    }

    cal_data_t * cal = &config_data.cals[adc];

    for(unsigned n = 0; n < count; n++)
        cal->cals[n] = cals[n].raw;

    cal->poly_len = count;

    if (unit)
    {
        unsigned len = strlen(unit) + 1;

        memcpy(&config_data.units[adc], unit, (len < sizeof(cal_unit_t))?len:sizeof(cal_unit_t));
        config_data.units[adc][sizeof(cal_unit_t)-1] = 0;
    }

    _persistent_commit();

    return true;
}


bool        persistent_set_use_cal(bool enable)
{
    if (!config_data_valid)
        return false;

    config_data.flags.use_adc_cals = (enable)?1:0;

    _persistent_commit();

    return true;
}


bool        persistent_get_use_cal()
{
    if (!config_data_valid)
        return false;

    return config_data.flags.use_adc_cals;
}
