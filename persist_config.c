#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/flash.h>

#include "log.h"
#include "persist_config.h"
#include "config.h"
#include "pinmap.h"

#define FLASH_ADDRESS 0x8000000
#define FLASH_PAGE_SIZE 2048
#define RAW_PERSIST_DATA ((void*)0x801F800)
#define RAW_PERSIST_SIZE 2048

#define CONFIG_NAME_MAX_LEN 32

typedef struct
{
    uint64_t offset;
    uint64_t scale;
} __attribute__((packed)) cal_data_t;

typedef char cal_unit_t[4];

typedef union
{
    struct
    {
        uint32_t  use_adc_cals:1;
    };
    uint32_t flags;
} __attribute__((packed)) config_flags_t;


typedef struct
{
    char       config_name[CONFIG_NAME_MAX_LEN];
    cal_data_t cals[ADC_COUNT];
    cal_unit_t units[ADC_COUNT];
    config_flags_t flags;
    uint16_t       crc;
} __attribute__((packed)) config_data_t;


static config_data_t * config_data = NULL;


static uint16_t _persistent_get_crc()
{
    crc_set_initial(0xBEEFBEEF);
    return crc_calculate_block(RAW_PERSIST_DATA, sizeof(config_data_t) - sizeof(uint16_t));
}


void        init_persistent()
{
    crc_set_polysize(16);
    crc_set_polynomial(0x4C11DB7);

    config_data = (config_data_t*)RAW_PERSIST_DATA;

    if (!config_data->config_name[0])
    {
        log_error("Persistent data empty.");
        config_data = NULL;
        return;
    }

    for(unsigned n = 0; n < 32; n++)
    {
        if (!isascii(config_data->config_name[n]))
        {
            log_error("Persistent data not valid");
            config_data = NULL;
            return;
        }

        if (!config_data->config_name[n])
        {
            log_out("Found named persistent data : %s", config_data->config_name);
            break;
        }
    }

    uint16_t crc = _persistent_get_crc();

    if (config_data->crc != crc)
    {
        log_error("Persistent data CRC failed");
        config_data = NULL;
    }
}


const char* persistent_get_name()
{
    if (config_data)
        return config_data->config_name;
    return NULL;
}


static void _persistent_update_crc()
{
    flash_program_half_word((uintptr_t)&config_data->crc, _persistent_get_crc());
}


static void persistent_set_data(const void * addr, const void * data, unsigned used_size, unsigned full_size)
{
    const uint8_t * p = (const uint8_t*)data;
    uintptr_t _addr = (uintptr_t)addr;

    if (used_size > full_size)
        used_size = full_size;

    for(unsigned n = 0; n < used_size; n+=2)
    {
        uint16_t v;
        if (n == (full_size - 1) || (n + 1) == used_size) /* Make sure we don't copy too much.*/
            v = p[n];
        else
            v = p[n] | (p[n + 1] << 8);

        flash_program_half_word(_addr + n, v);
    }
}


static void persistent_zero(const void * addr, unsigned size)
{
    for(unsigned n = 0; n < size; n+=2)
        flash_program_half_word(((uintptr_t)addr) + n, 0);
}


void        persistent_set_name(const char * name)
{
    flash_unlock();

    unsigned len = strlen(name) + 1;

    if (len > CONFIG_NAME_MAX_LEN)
    {
        log_error("Persistent name too long.");
        len = CONFIG_NAME_MAX_LEN;
    }

    flash_erase_page((uintptr_t)RAW_PERSIST_DATA);

    if (!config_data)
        config_data = (config_data_t*)RAW_PERSIST_DATA;

    persistent_set_data(config_data->config_name,
                        (const uint8_t*)name,
                        len, CONFIG_NAME_MAX_LEN);

    if (len % 2)
        len--;

    /*Zero fill any unused space.*/
    persistent_zero(config_data->config_name + len, CONFIG_NAME_MAX_LEN - len);


    /* Default calibration values are just 3.3v */
    basic_fixed_t temp;
    basic_fixed_t v3_3;

    basic_fixed_set_whole(&v3_3, 33);
    basic_fixed_set_whole(&temp, 10);
    basic_fixed_div(&v3_3, &v3_3, &temp); /* 3.3v */

    basic_fixed_set_whole(&temp, 1024);
    basic_fixed_div(&v3_3, &v3_3, &temp); /* 3.3v / 1024 */

    basic_fixed_set_whole(&temp, 0);

    for(unsigned n = 0; n < ADC_COUNT; n++)
    {
        cal_data_t * cal = &config_data->cals[n];
        persistent_set_data(&cal->scale, &v3_3, sizeof(uint64_t), sizeof(uint64_t));
        persistent_set_data(&cal->offset, &temp, sizeof(uint64_t), sizeof(uint64_t)); 
        persistent_set_data(&config_data->units[n], "V", 2, sizeof(cal_unit_t));
    }

    persistent_zero(&config_data->flags, sizeof(config_flags_t));

    _persistent_update_crc();

    flash_lock();
}


bool        persistent_get_cal(unsigned adc, basic_fixed_t * scale, basic_fixed_t * offset, const char ** unit)
{
    if (adc >= ADC_COUNT || !config_data ||  !offset || !scale || !unit)
        return false;

    offset->raw = config_data->cals[adc].offset;
    scale->raw = config_data->cals[adc].scale;
    *unit = config_data->units[adc];
    return true;
}


bool        persistent_set_cal(unsigned adc, basic_fixed_t * scale, basic_fixed_t * offset, const char * unit)
{
    if (adc >= ADC_COUNT || !config_data)
        return false;

    flash_unlock();

    cal_data_t * cal = &config_data->cals[adc];

    if (unit)
    {
        unsigned len = strlen(unit) + 1;
        if (len > sizeof(cal_unit_t))
            persistent_set_data(&config_data->units[adc],
                                unit,
                                sizeof(cal_unit_t) - 1,
                                sizeof(cal_unit_t));
        else
            persistent_set_data(&config_data->units[adc],
                                unit, len,
                                sizeof(cal_unit_t));
    }

    if (scale)
        persistent_set_data(&cal->scale, scale,
                            sizeof(uint64_t), sizeof(uint64_t));

    if (offset)
        persistent_set_data(&cal->offset, offset,
                            sizeof(uint64_t), sizeof(uint64_t));


    _persistent_update_crc();

    flash_lock();

   return true;
}


bool        persistent_set_use_cal(bool enable)
{
    if (!config_data)
        return false;

    flash_unlock();

    config_flags_t flags = config_data->flags;
    flags.use_adc_cals = (enable)?1:0;

    persistent_set_data(&config_data->flags, &flags,
                        sizeof(config_flags_t), sizeof(config_flags_t));
    flash_lock();

    return true;
}


bool        persistent_get_use_cal()
{
    if (!config_data)
        return false;

    return config_data->flags.use_adc_cals;
}
