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


#define htu21d_debug(...)  log_debug(DEBUG_TMP_HUM, "HTU21D: " __VA_ARGS__)



static bool htu21d_read_reg16(htu21d_reg_t reg, uint16_t * r)
{
    if (!r)
        return false;
    uint8_t reg8 = reg;
    uint8_t d[3] = {0};
    htu21d_debug("Read command 0x%"PRIx8, reg8);

    i2c_transfer7(HTU21D_I2C, I2C_HTU21D_R_ADDR, &reg8, 1, d, 3);

    htu21d_debug("Got 0x%"PRIx8" 0x%"PRIx8" 0x%"PRIx8, d[0], d[1], d[2]);

    *r = ((d[0] & 0x3f) << 8) | d[1];

    // HTU21D(F) sensor provides a CRC-8 checksum for error detection. The polynomial used is X8 + X5 + X4 + 1.
    return (d[2] == crc8(d, 2));
}


static void htu21d_send(htu21d_reg_t reg)
{
    uint8_t reg8 = reg;
    htu21d_debug("Send command 0x%"PRIx8, reg8);
    i2c_transfer7(HTU21D_I2C, I2C_HTU21D_W_ADDR, &reg8, 1, NULL, 0);
}



void htu21d_init()
{
    i2c_init(HTU21D_I2C_INDEX);
    htu21d_send(HTU21D_SOFT_RESET);
}


bool htu21d_read_temp(uint16_t * temp)
{
    return htu21d_read_reg16(HTU21D_HOLD_TRIG_TEMP_MEAS, temp);
}


bool htu21d_read_humidity(uint16_t * humidity)
{
    return htu21d_read_reg16(HTU21D_HOLD_TRIG_HUMI_MEAS, humidity);
}
