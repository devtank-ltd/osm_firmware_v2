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


typedef enum
{
    HTU21D_STATUS_OPENCIRCUIT   = 0,
    HTU21D_STATUS_TEMP          = 0,
    HTU21D_STATUS_HUMI          = 2,
    HTU21D_STATUS_CLOSEDCIRCUIT = 3,
} htu21d_status_t;


uint8_t crc8(uint8_t* mem, uint8_t size)
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
                crc ^= 0x8C;
            byte >>= 1;
         }
    }
    return crc;
}


static bool htu21d_read_reg16(htu21d_reg_t reg, uint16_t * r)
{
    if (!r)
        return false;
    uint8_t reg8 = reg;
    uint8_t d[3] = {0};
    i2c_transfer7(HTU21D_I2C, I2C_HTU21D_R_ADDR, &reg8, 1, d, 3);
    htu21d_status_t stat = d[1] >> 6;
    if (reg == HTU21D_TRIG_TEMP_MEAS || reg == HTU21D_HOLD_TRIG_TEMP_MEAS)
    {
        if (stat != HTU21D_STATUS_TEMP)
            return false;
    }
    else if (reg == HTU21D_TRIG_HUMI_MEAS || reg == HTU21D_HOLD_TRIG_HUMI_MEAS)
    {
        if (stat != HTU21D_STATUS_HUMI)
            return false;
    }
    *r = (d[0] << 6) | (d[1] & 0x3f);

    // HTU21D(F) sensor provides a CRC-8 checksum for error detection. The polynomial used is X8 + X5 + X4 + 1.
    return (d[2] == crc8(d, 2));
}


void htu21d_init()
{
    i2c_init(HTU21D_I2C_INDEX);
}


bool htu21d_read_temp(uint16_t * temp)
{
    return htu21d_read_reg16(HTU21D_HOLD_TRIG_TEMP_MEAS, temp);
}


bool htu21d_read_humidity(uint16_t * humidity)
{
    return htu21d_read_reg16(HTU21D_HOLD_TRIG_HUMI_MEAS, humidity);
}
