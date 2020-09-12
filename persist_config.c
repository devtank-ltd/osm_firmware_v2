#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "log.h"
#include "persist_config.h"
#include "config.h"
#include "pinmap.h"

#define raw_config_data ((void*)0x801f800)


typedef struct
{
    uint64_t offset;
    uint64_t scale;
} cal_data_t;


typedef struct
{
    char       config_name[32];
    cal_data_t cals[ADC_COUNT];
    uint16_t   checksum;
} config_data_t;



static config_data_t * config_data = NULL;

void        init_persistent()
{
    config_data = (config_data_t*)raw_config_data;

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
}


const char* persistent_get_name()
{
    if (config_data)
        return config_data->config_name;
    return NULL;
}


void        persistent_set_name(const char * name[32])
{
    if (!config_data)
        config_data = (config_data_t*)raw_config_data;

    memset(config_data->config_name, 0, 32);
    memcpy(config_data->config_name, name, strlen((char*)name));
}


bool        persistent_get_cal(unsigned adc, int32_t * scale, int32_t * offset)
{
    adc = adc;
    scale = scale;
    offset = offset;
    return false;
}


bool        persistent_set_cal(unsigned adc, int32_t scale, int32_t offset)
{
    adc = adc;
    scale = scale;
    offset = offset;
    return false;
}
