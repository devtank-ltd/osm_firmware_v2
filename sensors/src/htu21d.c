/*
A driver for a HPP845E131R5 temperature and humidity sensor by Devtank Ltd.

Documents used:
- HPP845E131R5 Temperature and Humidity Sensor
    : https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FHPC199_6%7FA6%7Fpdf%7FEnglish%7FENG_DS_HPC199_6_A6.pdf%7FHPP845E131R5
      (Accessed: 10.01.22)
*/


#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "config.h"
#include "i2c.h"
#include "log.h"
#include "htu21d.h"
#include "measurements.h"

#define MEASUREMENT_COLLECTION_MS   150
#define HTU21D_PAUSE_TIME           50


typedef enum
{
    HTU21D_STATE_FLAG_NONE        = 0x0,
    HTU21D_STATE_FLAG_TEMPERATURE = 0x1,
    HTU21D_STATE_FLAG_HUMIDITY    = 0x2,
} htu21d_flags_t;


typedef enum
{
    HTU21D_HOLD_TRIG_TEMP_MEAS = 0xE3,
    HTU21D_HOLD_TRIG_HUMI_MEAS = 0xE5,
    HTU21D_TRIG_TEMP_MEAS      = 0xF3,
    HTU21D_TRIG_HUMI_MEAS      = 0xF5,
    HTU21D_WRITE_USER_REG      = 0xE6,
    HTU21D_READ_USER_REG       = 0xE7,
    HTU21D_SOFT_RESET          = 0xFE,
} htu21d_reg_t;


typedef enum
{
    HTU21D_VALUE_STATE_IDLE,
    HTU21D_VALUE_STATE_REQ ,
} htu21d_value_state_t;


typedef struct
{
    uint8_t flags;
} htu21d_state;


typedef struct
{
    int32_t                 value;
    uint32_t                time_requested;
    htu21d_value_state_t    state;
    bool                    is_valid;
} htu21d_value_t;


typedef struct
{
    htu21d_value_t temperature;
    htu21d_value_t humidity;
} htu21d_reading_t;


static htu21d_reading_t _htu21d_reading = {.temperature={0, 0, false},
                                           .humidity={0, 0, false}};


static htu21d_state _htu21d_state_machine = {.flags=HTU21D_STATE_FLAG_NONE};


// HTU21D(F) sensor provides a CRC-8 checksum for error detection. The polynomial used is X8 + X5 + X4 + 1.
uint8_t _crc8(const uint8_t* mem, uint8_t size)
{
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < size; i++)
    {
         uint8_t byte = mem[i];
         crc ^= byte;
         for (uint8_t j = 0; j < 8; ++j)
         {
            if (crc & 0x80)
            {
                crc <<= 1;
                crc ^= 0x131;
            }
            else crc <<= 1;
         }
    }
    return crc;
}


#define htu21d_debug(...)  log_debug(DEBUG_TMP_HUM, "HTU21D: " __VA_ARGS__)



static bool _htu21d_get_u16(uint8_t d[3], uint16_t *r)
{
    htu21d_debug("Got 0x%"PRIx8" 0x%"PRIx8" 0x%"PRIx8, d[0], d[1], d[2]);

    *r = (d[0] << 8) | d[1];

    uint8_t crc =  _crc8(d, 2);
    htu21d_debug("CRC : 0x%"PRIx8, crc);
    return d[2] == crc;
}


static void _htu21d_send(htu21d_reg_t reg)
{
    uint8_t reg8 = reg;
    htu21d_debug("Send command 0x%"PRIx8, reg8);
    if (!i2c_transfer_timeout(I2C_TYPE_HTU21D, &reg8, 1, NULL, 0, 100) && reg != HTU21D_SOFT_RESET)
        htu21d_init();
}


static bool _htu21d_read_data(uint16_t *r, uint32_t timeout)
{
    htu21d_debug("Try read");
    uint8_t d[3] = {0};
    if (!i2c_transfer_timeout(I2C_TYPE_HTU21D, NULL, 0, d, 3, timeout))
    {
        htu21d_debug("Read timeout.");
        htu21d_init();
        return false;
    }
    return _htu21d_get_u16(d, r);
}


static bool _htu21d_temp_conv(uint16_t s_temp, int32_t* temp)
{
    if (!temp)
    {
        return false;
    }
    *temp = 17572 * s_temp / (1 << 16) - 4685;
    return true;
}


static bool _htu21d_humi_conv(uint16_t s_humi, int32_t* humi)
{
    if (!humi)

        return false;
    *humi =12500 * s_humi / (1 << 16) - 600;
    return true;
}


static bool _htu21d_humi_full(int32_t temp, uint16_t s_humi, int32_t* humi)
{
    if (!_htu21d_humi_conv(s_humi, humi))
    {
        return false;
    }
    *humi += -(15 * (2500 - temp)) / 100;
    return true;
}


measurements_sensor_state_t htu21d_measurements_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = MEASUREMENT_COLLECTION_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static void _htu21d_iteration_loop_req_temp(void)
{
    _htu21d_send(HTU21D_HOLD_TRIG_TEMP_MEAS);
    _htu21d_reading.temperature.time_requested = get_since_boot_ms();
    _htu21d_reading.temperature.state = HTU21D_VALUE_STATE_REQ;
}


static void _htu21d_iteration_loop_req_humi(void)
{
    _htu21d_send(HTU21D_HOLD_TRIG_HUMI_MEAS);
    _htu21d_reading.humidity.time_requested = get_since_boot_ms();
    _htu21d_reading.humidity.state = HTU21D_VALUE_STATE_REQ;
}


measurements_sensor_state_t htu21d_temp_measurements_init(char* name)
{
    if (_htu21d_state_machine.flags & HTU21D_STATE_FLAG_TEMPERATURE)
    {
        htu21d_debug("Cannot queue temperature, already requested.");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }
    _htu21d_state_machine.flags |= HTU21D_STATE_FLAG_TEMPERATURE;
    if (_htu21d_state_machine.flags & HTU21D_STATE_FLAG_HUMIDITY)
    {
        htu21d_debug("Queuing temperature.");
        return MEASUREMENTS_SENSOR_STATE_SUCCESS;
    }
    htu21d_debug("Requesting temperature.");
    _htu21d_iteration_loop_req_temp();
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t htu21d_humi_measurements_init(char* name)
{
    if (_htu21d_state_machine.flags & HTU21D_STATE_FLAG_HUMIDITY)
    {
        htu21d_debug("Cannot queue humidity, already requested.");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }
    _htu21d_state_machine.flags |= HTU21D_STATE_FLAG_HUMIDITY;
    if (_htu21d_state_machine.flags & HTU21D_STATE_FLAG_TEMPERATURE)
    {
        htu21d_debug("Queuing humidity.");
        return MEASUREMENTS_SENSOR_STATE_SUCCESS;
    }
    htu21d_debug("Requesting humidity.");
    _htu21d_iteration_loop_req_temp();
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static bool _htu21d_iteration_loop_collect_temp(void)
{
    _htu21d_reading.temperature.state = HTU21D_VALUE_STATE_IDLE;
    uint16_t s_temp;
    if (!_htu21d_read_data(&s_temp, 10))
    {
        htu21d_debug("Could not read temperature data.");
        return false;
    }
    if (!_htu21d_temp_conv(s_temp, &_htu21d_reading.temperature.value))
    {
        htu21d_debug("Could not convert temperature.");
        return false;
    }
    _htu21d_reading.temperature.is_valid = true;
    htu21d_debug("temperature: %i.%02udegC", (int)_htu21d_reading.temperature.value/100, (unsigned)abs(_htu21d_reading.temperature.value%100));
    return true;
}


static bool _htu21d_iteration_loop_collect_humi(void)
{
    _htu21d_reading.humidity.state = HTU21D_VALUE_STATE_IDLE;
    uint16_t s_humi;
    if (!_htu21d_reading.temperature.is_valid)
    {
        htu21d_debug("Temperature value is invalid.");
        return false;
    }
    if (!_htu21d_read_data(&s_humi, 10))
    {
        htu21d_debug("Could not read humidity data.");
        return false;
    }
    if (!_htu21d_humi_full(_htu21d_reading.temperature.value, s_humi, &_htu21d_reading.humidity.value))
    {
        htu21d_debug("Could not convert humidity.");
        return false;
    }
    _htu21d_reading.humidity.is_valid = true;
    htu21d_debug("Collected humidity into buffer.");
    htu21d_debug("Humidity: %i.%02u%%", (int)_htu21d_reading.humidity.value/100, (unsigned)abs(_htu21d_reading.humidity.value%100));
    return true;
}


measurements_sensor_state_t htu21d_measurements_iteration(char* name)
{
    uint8_t flags = _htu21d_state_machine.flags;
    // Both
    if (flags & HTU21D_STATE_FLAG_TEMPERATURE && flags & HTU21D_STATE_FLAG_HUMIDITY)
    {
        if (_htu21d_reading.temperature.state == HTU21D_VALUE_STATE_REQ &&
            since_boot_delta(get_since_boot_ms(), _htu21d_reading.temperature.time_requested) > HTU21D_PAUSE_TIME)
        {
            if (!_htu21d_iteration_loop_collect_temp())
                goto bad_temp_exit;
            htu21d_debug("Collected temperature into buffer, triggering humidity.");
            _htu21d_iteration_loop_req_humi();
        }
        if (_htu21d_reading.humidity.state == HTU21D_VALUE_STATE_REQ &&
            since_boot_delta(get_since_boot_ms(), _htu21d_reading.humidity.time_requested) > HTU21D_PAUSE_TIME)
        {
            if (!_htu21d_iteration_loop_collect_humi())
                goto bad_humi_exit;
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
    }
    // Temp
    else if (flags & HTU21D_STATE_FLAG_TEMPERATURE && !(flags & HTU21D_STATE_FLAG_HUMIDITY))
    {
        if (_htu21d_reading.temperature.state == HTU21D_VALUE_STATE_REQ &&
            since_boot_delta(get_since_boot_ms(), _htu21d_reading.temperature.time_requested) > HTU21D_PAUSE_TIME)
        {
            if (!_htu21d_iteration_loop_collect_temp())
                goto bad_temp_exit;
            htu21d_debug("Collected temperature into buffer.");
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
    }
    // Humi
    else if (flags & HTU21D_STATE_FLAG_HUMIDITY && !(flags & HTU21D_STATE_FLAG_TEMPERATURE))
    {
        if (_htu21d_reading.temperature.state == HTU21D_VALUE_STATE_REQ &&
            since_boot_delta(get_since_boot_ms(), _htu21d_reading.temperature.time_requested) > HTU21D_PAUSE_TIME)
        {
            if (!_htu21d_iteration_loop_collect_temp())
                goto bad_temp_exit;
            htu21d_debug("Collected temperature into buffer, triggering humidity.");
            _htu21d_iteration_loop_req_humi();
        }
        if (_htu21d_reading.humidity.state == HTU21D_VALUE_STATE_REQ &&
            since_boot_delta(get_since_boot_ms(), _htu21d_reading.humidity.time_requested) > HTU21D_PAUSE_TIME)
        {
            if (!_htu21d_iteration_loop_collect_humi())
            {
                _htu21d_reading.temperature.is_valid = false;
                goto bad_humi_exit;
            }
            _htu21d_reading.temperature.is_valid = false;
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
    }
    return MEASUREMENTS_SENSOR_STATE_BUSY;

bad_temp_exit:
    if (strncmp(name, MEASUREMENTS_HTU21D_TEMP, MEASURE_NAME_LEN) == 0)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    return MEASUREMENTS_SENSOR_STATE_BUSY;
bad_humi_exit:
    if (strncmp(name, MEASUREMENTS_HTU21D_HUMI, MEASURE_NAME_LEN) == 0)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    return MEASUREMENTS_SENSOR_STATE_BUSY;
}


measurements_sensor_state_t htu21d_temp_measurements_get(char* name, measurements_reading_t* value)
{
    uint8_t flags = _htu21d_state_machine.flags;
    _htu21d_state_machine.flags &= ~HTU21D_STATE_FLAG_TEMPERATURE;
    if (!value)
    {
        htu21d_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!(flags & HTU21D_STATE_FLAG_TEMPERATURE))
    {
        htu21d_debug("Cannot get temperature, flag not set.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!_htu21d_reading.temperature.is_valid)
    {
        htu21d_debug("Temperature value is not valid (old).");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_i64 = (int64_t)_htu21d_reading.temperature.value;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t htu21d_humi_measurements_get(char* name, measurements_reading_t* value)
{
    uint8_t flags = _htu21d_state_machine.flags;
    _htu21d_state_machine.flags &= ~HTU21D_STATE_FLAG_HUMIDITY;
    if (!value)
    {
        htu21d_debug("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!(flags & HTU21D_STATE_FLAG_HUMIDITY))
    {
        htu21d_debug("Cannot get humidity, flag not set.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!_htu21d_reading.humidity.is_valid)
    {
        htu21d_debug("Temperature or humidity value is not valid (old).");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_i64 = (int64_t)_htu21d_reading.humidity.value;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void htu21d_init(void)
{
    _htu21d_send(HTU21D_SOFT_RESET);
}


bool htu21d_read_temp(int32_t* temp)
{
    if (!temp)
    {
        htu21d_debug("Handed a NULL pointer for temperature.");
        return false;
    }
    if (htu21d_temp_measurements_init(MEASUREMENTS_HTU21D_HUMI) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        htu21d_debug("Failed to begin the temperature measurement.");
        return false;
    }

    uint32_t timeout;
    if (htu21d_measurements_collection_time(MEASUREMENTS_HTU21D_HUMI, &timeout) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        htu21d_debug("Failed to get the temperature collection time.");
        return false;
    }

    uint32_t start = get_since_boot_ms();
    while (since_boot_delta(get_since_boot_ms(), start) < timeout)
    {
        /* Watchdog? */
        if (htu21d_measurements_iteration(MEASUREMENTS_HTU21D_HUMI) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
        {
            htu21d_debug("Failed on temperature iteration.");
            return false;
        }
    }

    measurements_reading_t reading;
    if (htu21d_temp_measurements_get(MEASUREMENTS_HTU21D_HUMI, &reading) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        htu21d_debug("Could not collect the temperature measurement.");
        return false;
    }

    *temp = reading.v_i64;
    return true;
}


bool htu21d_read_humidity(int32_t* humidity)
{
    if (!humidity)
    {
        htu21d_debug("Handed a NULL pointer for humidity.");
        return false;
    }
    if (htu21d_humi_measurements_init(MEASUREMENTS_HTU21D_HUMI) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        htu21d_debug("Failed to begin the humidity measurement.");
        return false;
    }

    uint32_t timeout;
    if (htu21d_measurements_collection_time(MEASUREMENTS_HTU21D_HUMI, &timeout) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        htu21d_debug("Failed to get the humidity collection time.");
        return false;
    }

    uint32_t start = get_since_boot_ms();
    while (since_boot_delta(get_since_boot_ms(), start) < timeout)
    {
        /* Watchdog? */
        if (htu21d_measurements_iteration(MEASUREMENTS_HTU21D_HUMI) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
        {
            htu21d_debug("Failed on humidity iteration.");
            return false;
        }
    }

    measurements_reading_t reading;
    if (htu21d_humi_measurements_get(MEASUREMENTS_HTU21D_HUMI, &reading) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        htu21d_debug("Could not collect the humidity measurement.");
        return false;
    }

    *humidity = reading.v_i64;
    return true;
}
