/*
A driver for a DS18B20 thermometer by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf                          (Accessed: 25.03.21)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html     (Accessed: 25.03.21)
*/
#include <string.h>

#include "pinmap.h"
#include "log.h"
#include "common.h"
#include "io.h"
#include "w1.h"
#include "ds18b20.h"


#define DS18B20_INSTANCES   {                                          \
    { { MEASUREMENTS_W1_PROBE_NAME_1, W1_PULSE_1_IO} ,                 \
      { { W1_PULSE_PORT, W1_PULSE_1_PIN } } }                          \
}

#define DS18B20_DELAY_GET_TEMP_US   750000

#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONV_T          0x44
#define DS18B20_CMD_READ_SCP        0xBE

#define DS18B20_DEFAULT_COLLECTION_TIME_MS DS18B20_DELAY_GET_TEMP_US/1000


typedef union
{
    struct
    {
        uint16_t t;
        uint16_t tmp_t;
        uint8_t conf;
        uint8_t res0;
        uint8_t res1;
        uint8_t res2;
        uint8_t crc;
    } __attribute__((packed));
    uint8_t raw[9];
} ds18b20_memory_t;


typedef struct
{
    special_io_info_t   info;
    w1_instance_t       w1_instance;
} ds18b20_instance_t;


static ds18b20_instance_t _ds18b20_instances[] = DS18B20_INSTANCES;


static void _ds18b20_read_scpad(ds18b20_memory_t* d, w1_instance_t* instance)
{
    for (int i = 0; i < 9; i++)
    {
        d->raw[i] = w1_read_byte(instance);
    }
}


static void _ds18b20_delay_us(uint32_t delay)
{
    uint32_t start = get_since_boot_ms();
    while (since_boot_delta(get_since_boot_ms(), start) < (delay/1000));
    return;
}


static bool _ds18b20_crc_check(const uint8_t* mem, uint8_t size)
{
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < size; i++)
    {
         uint8_t byte = mem[i];
         for (uint8_t j = 0; j < 8; ++j)
         {
            uint8_t blend = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (blend)
            {
                crc ^= 0x8C;
            }
            byte >>= 1;
         }
    }
    if (crc == mem[size])
    {
        return true;
    }
    else
    {
        return false;
    }
}


static bool _ds18b20_empty_check(const uint8_t* mem, unsigned size)
{
    for (unsigned i = 0; i < size; i++)
    {
        if (mem[i] != 0)
        {
            return true;
        }
    }
    return false;
}


void ds18b20_enable(bool enable)
{
}


static bool _ds18b20_get_instance(ds18b20_instance_t** instance, char* name)
{
    ds18b20_instance_t* inst;
    for (unsigned i = 0; i < ARRAY_SIZE(_ds18b20_instances); i++)
    {
        inst = &_ds18b20_instances[i];
        if (strncmp(name, inst->info.name, sizeof(inst->info.name) * sizeof(char)) == 0)
        {
            *instance = inst;
            return true;
        }
    }
    return false;
}


bool ds18b20_query_temp(float* temperature, char* name)
{
    ds18b20_instance_t* instance;
    if (!_ds18b20_get_instance(&instance, name))
    {
        exttemp_debug("Cannot get instance.");
        return false;
    }
    if (!io_is_w1_now(instance->info.io))
    {
        exttemp_debug("IO not set.");
        return false;
    }

    ds18b20_memory_t d; 
    if (!w1_reset(&instance->w1_instance))
    {
        goto bad_exit;
    }
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_SKIP_ROM);
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_CONV_T);
    _ds18b20_delay_us(DS18B20_DELAY_GET_TEMP_US);
    if (!w1_reset(&instance->w1_instance))
    {
        goto bad_exit;
    }
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_SKIP_ROM);
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_READ_SCP);
    _ds18b20_read_scpad(&d, &instance->w1_instance);

    if (!_ds18b20_crc_check(d.raw, 8))
    {
        exttemp_debug("Temperature data not confirmed by CRC");
        return false;
    }

    if (!_ds18b20_empty_check(d.raw, 9))
    {
        exttemp_debug("Empty memory.");
        return false;
    }

    int16_t integer_bits = d.t >> 4;
    int8_t decimal_bits = d.t & 0x000F;
    if (d.t & 0x8000)
    {
        integer_bits = integer_bits | 0xF000;
        decimal_bits = (decimal_bits - 16) % 16;
    }

    *temperature = (float)integer_bits + (float)decimal_bits / 16.f;
    exttemp_debug("T: %.03f C", *temperature);
    return true;
bad_exit:
    exttemp_debug("Temperature probe did not respond");
    return false;
}


measurements_sensor_state_t ds18b20_measurements_init(char* name)
{
    ds18b20_instance_t* instance;
    if (!_ds18b20_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!io_is_w1_now(instance->info.io))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!w1_reset(&instance->w1_instance))
    {
        exttemp_debug("Temperature probe did not respond");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_SKIP_ROM);
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_CONV_T);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t ds18b20_measurements_collect(char* name, measurements_reading_t* value)
{
    ds18b20_instance_t* instance;
    if (!_ds18b20_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!io_is_w1_now(instance->info.io))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    ds18b20_memory_t d;
    if (!w1_reset(&instance->w1_instance))
    {
        exttemp_debug("Temperature probe did not respond");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_SKIP_ROM);
    w1_send_byte(&instance->w1_instance, DS18B20_CMD_READ_SCP);
    _ds18b20_read_scpad(&d, &instance->w1_instance);

    if (!_ds18b20_crc_check(d.raw, 8))
    {
        exttemp_debug("Data not confirmed by CRC");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!_ds18b20_empty_check(d.raw, 9))
    {
        exttemp_debug("Empty memory.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    int16_t integer_bits = d.t >> 4;
    int8_t decimal_bits = d.t & 0x000F;
    if (d.t & 0x8000)
    {
        integer_bits = integer_bits | 0xF000;
        decimal_bits = (decimal_bits - 16) % 16;
    }

    value->v_f32 = (int32_t)integer_bits * 1000 + ((int32_t)decimal_bits * 1000) / 16;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t ds18b20_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = DS18B20_DEFAULT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static void _ds18b20_init_instance(ds18b20_instance_t* instance)
{
    rcc_periph_clock_enable(PORT_TO_RCC(W1_PULSE_PORT));
}


void ds18b20_temp_init(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_ds18b20_instances); i++)
    {
        _ds18b20_init_instance(&_ds18b20_instances[i]);
    }
}
