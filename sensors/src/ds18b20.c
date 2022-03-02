/*
A driver for a DS18B20 thermometer by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf                          (Accessed: 25.03.21)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html     (Accessed: 25.03.21)
*/
#include "pinmap.h"
#include "log.h"
#include "value.h"
#include "w1.h"
#include "ds18b20.h"

#define DS18B20_DELAY_GET_TEMP   750000

#define DS18B20_CMD_SKIP_ROM     0xCC
#define DS18B20_CMD_CONV_T       0x44
#define DS18B20_CMD_READ_SCP     0xBE

#define DS18B20_DEFAULT_COLLECTION_TIME_MS DS18B20_DELAY_GET_TEMP/1000


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


static bool _ds18b20_reset(void)
{
    return w1_reset(W1_PORT, W1_PIN);
}


static uint8_t _ds18b20_read_byte(void)
{
    return w1_read_byte(W1_PORT, W1_PIN);
}


static void _ds18b20_send_byte(uint8_t byte)
{
    return w1_send_byte(W1_PORT, W1_PIN, byte);
}


static void _ds18b20_read_scpad(ds18b20_memory_t* d)
{
    for (int i = 0; i < 9; i++)
    {
        d->raw[i] = _ds18b20_read_byte();
    }
}


static bool _ds18b20_crc_check(uint8_t* mem, uint8_t size)
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


static bool _ds18b20_empty_check(uint8_t* mem, unsigned size)
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


static void _ds18b20_temp_err(void)
{
    exttemp_debug("Temperature probe did not respond");
}


void ds18b20_enable(bool enable)
{
}


bool ds18b20_query_temp(float* temperature)
{
    if (!io_is_w1_now(W1_IO))
        return false;

    ds18b20_memory_t d; 
    if (!_ds18b20_reset())
    {
        _ds18b20_temp_err();
        return false;
    }
    _ds18b20_send_byte(DS18B20_CMD_SKIP_ROM);
    _ds18b20_send_byte(DS18B20_CMD_CONV_T);
    _ds18b20_delay_us(DELAY_GET_TEMP);
    if (!_ds18b20_reset())
    {
        _ds18b20_temp_err();
        return false;
    }
    _ds18b20_send_byte(W1_CMD_SKIP_ROM);
    _ds18b20_send_byte(W1_CMD_READ_SCP);
    _ds18b20_read_scpad(&d);

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
}


measurements_sensor_state_t ds18b20_measurements_init(char* name)
{
    if (!io_is_w1_now(W1_IO))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!_ds18b20_reset())
    {
        _ds18b20_temp_err();
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    _ds18b20_send_byte(DS18B20_CMD_SKIP_ROM);
    _ds18b20_send_byte(DS18B20_CMD_CONV_T);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t ds18b20_measurements_collect(char* name, value_t* value)
{
    if (!io_is_w1_now(W1_IO))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    ds18b20_memory_t d;
    if (!_ds18b20_reset())
    {
        _ds18b20_temp_err();
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    _ds18b20_send_byte(DS18B20_CMD_SKIP_ROM);
    _ds18b20_send_byte(DS18B20_CMD_READ_SCP);
    _ds18b20_read_scpad(&d);

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

    *value = value_from_float((float)integer_bits + (float)decimal_bits / 16.f);
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


void ds18b20_temp_init(void)
{
    _ds18b20_reset();
}
