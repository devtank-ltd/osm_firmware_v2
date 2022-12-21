/*
A driver for a DS18B20 thermometer by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf                          (Accessed: 25.03.21)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html     (Accessed: 25.03.21)
*/
#include <string.h>

#include "log.h"
#include "common.h"
#include "io.h"
#include "w1.h"
#include "ds18b20.h"
#include "pinmap.h"

#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONV_T          0x44
#define DS18B20_CMD_READ_SCP        0xBE

#define DS18B20_DEFAULT_COLLECTION_TIME_MS 750


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
    uint8_t             w1_index;
} ds18b20_instance_t;


static ds18b20_instance_t _ds18b20_instances[] = DS18B20_INSTANCES;


static void _ds18b20_read_scpad(ds18b20_memory_t* d, ds18b20_instance_t* instance)
{
    for (int i = 0; i < 9; i++)
    {
        d->raw[i] = w1_read_byte(instance->w1_index);
    }
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


static measurements_sensor_state_t _ds18b20_measurements_init(char* name, bool in_isolation)
{
    ds18b20_instance_t* instance;
    if (!_ds18b20_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    log_out("io = %u", instance->w1_index);

    if (!w1_reset(instance->w1_index))
    {
        exttemp_debug("Temperature probe did not respond");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    w1_send_byte(instance->w1_index, DS18B20_CMD_SKIP_ROM);
    w1_send_byte(instance->w1_index, DS18B20_CMD_CONV_T);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _ds18b20_measurements_collect(char* name, measurements_reading_t* value)
{
    ds18b20_instance_t* instance;
    if (!_ds18b20_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    ds18b20_memory_t d;
    if (!w1_reset(instance->w1_index))
    {
        exttemp_debug("Temperature probe did not respond");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    w1_send_byte(instance->w1_index, DS18B20_CMD_SKIP_ROM);
    w1_send_byte(instance->w1_index, DS18B20_CMD_READ_SCP);
    _ds18b20_read_scpad(&d, instance);

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
        integer_bits = (integer_bits | 0xF000) + 1;
        decimal_bits = (decimal_bits - 16) % 16;
        if (!decimal_bits)
            integer_bits--;
    }
    float temperature = (float)integer_bits + (float)decimal_bits / 16.f;
    value->v_f32 = to_f32_from_float(temperature);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _ds18b20_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = DS18B20_DEFAULT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _ds18b20_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_FLOAT;
}


void                         ds18b20_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _ds18b20_collection_time;
    inf->init_cb            = _ds18b20_measurements_init;
    inf->get_cb             = _ds18b20_measurements_collect;
    inf->value_type_cb      = _ds18b20_value_type;
}


static void _ds18b20_init_instance(ds18b20_instance_t* instance)
{
    w1_init(instance->w1_index);
}


void ds18b20_temp_init(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_ds18b20_instances); i++)
    {
        _ds18b20_init_instance(&_ds18b20_instances[i]);
    }
}
