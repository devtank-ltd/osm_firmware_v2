#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/crc.h>

#include "log.h"
#include "persist_config.h"
#include "config.h"
#include "pinmap.h"

#define RAW_PERSIST_DATA ((void*)0x801f800)
#define RAW_PERSIST_SIZE 2048

typedef struct
{
    uint64_t offset;
    uint64_t scale;
} cal_data_t;


typedef struct
{
    char       config_name[32];
    cal_data_t cals[ADC_COUNT];
    uint16_t   crc;
} config_data_t;


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
    config_data->crc = _persistent_get_crc();
}


void        persistent_set_name(const char * name[32])
{
    if (!config_data)
    {
        config_data = (config_data_t*)RAW_PERSIST_DATA;
        memset(RAW_PERSIST_DATA, 0, RAW_PERSIST_SIZE);
    }

    memset(config_data->config_name, 0, 32);
    memcpy(config_data->config_name, name, strlen((char*)name));

    _persistent_update_crc();
}


bool        persistent_get_cal(unsigned adc, int32_t * scale, int32_t * offset)
{
    if (adc >= ADC_COUNT || !config_data ||  !offset || !scale)
        return false;

    *offset = config_data->cals[adc].offset;
    *scale = config_data->cals[adc].scale;
    return true;
}


bool        persistent_set_cal(unsigned adc, int32_t scale, int32_t offset)
{
    if (adc >= ADC_COUNT || !config_data)
        return false;

    config_data->cals[adc].offset = offset;
    config_data->cals[adc].scale  = scale;
    return true;
}
