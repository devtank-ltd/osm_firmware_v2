/*
A driver for a HPP845E131R5 temperature and humidity sensor by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FHPC199_6%7FA6%7Fpdf%7FEnglish%7FENG_DS_HPC199_6_A6.pdf%7FHPP845E131R5
      (Accessed: 10.01.22)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html
      (Accessed: 25.03.21)
*/


#include <inttypes.h>

#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "i2c.h"
#include "log.h"
#include "htu21d.h"

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


static int32_t nlz(uint32_t x)
{
    int32_t y, m, n;

    y = -(x >> 16);
    m = (y >> 16) & 16;
    n = 16 - m;
    x = x >> m;

    y = x - 0x100;
    m = (y >> 16) & 8;
    n = n + m;
    x = x << m;

    y = x - 0x1000;
    m = (y >> 16) & 4;
    n = n + m;
    x = x << m;

    y = x - 0x4000;
    m = (y >> 16) & 2;
    n = n + m;
    x = x << m;

    y = x >> 14;
    m = y & ~(y >> 1);
    return n + 2 - m;
}


int32_t ilog10(uint32_t x)
{
    int32_t y;
    static uint32_t table2[11] = {0, 9, 99, 999, 9999, 99999, 999999, 9999999, 99999999, 999999999, 0xFFFFFFFF};
    y = (19 * (31 - nlz(x))) >> 6;
    y = y + ((table2[y+1] - x) >> 31);
    return y;
}


// HTU21D(F) sensor provides a CRC-8 checksum for error detection. The polynomial used is X8 + X5 + X4 + 1.
uint8_t _crc8(uint8_t* mem, uint8_t size)
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


static bool _htu21d_read_reg16(htu21d_reg_t reg, uint16_t * r)
{
    if (!r)
        return false;
    uint8_t reg8 = reg;
    uint8_t d[3] = {0};
    htu21d_debug("Read command 0x%"PRIx8, reg8);

    i2c_transfer7(HTU21D_I2C, I2C_HTU21D_ADDR, &reg8, 1, d, 3);

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
    i2c_transfer7(HTU21D_I2C, I2C_HTU21D_ADDR, &reg8, 1, NULL, 0);
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


static bool _htu21d_dew_point(int32_t temp, int32_t humi, int32_t* t_dew)
{
    float A, B, C;
    A = 8.1332f;
    B = 1762.39f;
    C = 235.66f;
    uint32_t denom = ilog10(humi) + A - B / (temp/100.0f + C) - 4 - A;
    *t_dew = -B * 100 / denom;
    return true;
}


void htu21d_init(void)
{
    i2c_init(HTU21D_I2C_INDEX);
    _htu21d_send(HTU21D_SOFT_RESET);
}


bool htu21d_read_temp(int32_t* temp)
{
    uint16_t s_temp;
    if (!_htu21d_read_reg16(HTU21D_HOLD_TRIG_TEMP_MEAS, &s_temp))
    {
        htu21d_debug("Cannot read the temperature.");
        return false;
    }
    return _htu21d_temp_conv(s_temp, temp);
}


bool htu21d_read_humidity(int32_t* humidity)
{
    uint16_t s_humi;
    int32_t temp;

    if (!_htu21d_read_reg16(HTU21D_HOLD_TRIG_HUMI_MEAS, &s_humi))
    {
        htu21d_debug("Cannot read the humdity.");
        return false;
    }
    if (!htu21d_read_temp(&temp))
    {
        htu21d_debug("Cannot read the temperature.");
        return false;
    }
    return _htu21d_humi_full(temp, s_humi, humidity);
}


bool htu21d_read_dew_temp(int32_t* dew_temp)
{
    int32_t temp, humi;
    if (!htu21d_read_temp(&temp))
    {
        return false;
    }
    if (!htu21d_read_humidity(&humi))
    {
        return false;
    }
    return _htu21d_dew_point(temp, humi, dew_temp);
}
