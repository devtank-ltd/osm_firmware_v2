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
//    HTU21D_TRIG_TEMP_MEAS      = 0xF3,
//    HTU21D_TRIG_HUMI_MEAS      = 0xF5,
    HTU21D_WRITE_USER_REG      = 0xE6,
    HTU21D_READ_USER_REG       = 0xE7,
    HTU21D_SOFT_RESET          = 0xFE,
} htu21d_reg_t;

typedef enum
{
    HTU21D_STATUS_OPENCIRCUIT   = 0b00,
    HTU21D_STATUS_TEMP          = 0b00,
    HTU21D_STATUS_HUMI          = 0b10,
    HTU21D_STATUS_CLOSEDCIRCUIT = 0b11,
} htu21d_status_t;


static uint16_t htu21d_read_reg16(htu21d_reg_t reg, htu21d_status_t * stat )
{
    uint8_t reg8 = reg;
    uint8_t d[3] = 0;
    i2c_transfer7(HTU21D_I2C, I2C_HTU21D_R_ADDR, &reg8, 1, d, 3);
    *stat = d[1] >> 6;
    uint16_t r = (d[0] << 6) | (d[1] & 0b111111);
    // HTU21D(F) sensor provides a CRC-8 checksum for error detection. The polynomial used is X8 + X5 + X4 + 1.
    return r;
}


void htu21d_init()
{
    i2c_init(HTU21D_I2C_INDEX);
}


bool htu21d_read_temp(uint16_t * temp)
{
    if (!temp)
	return false;
    *temp = htu21d_read_reg16(HTU21D_HOLD_TRIG_TEMP_MEAS);
    return true;
}

bool htu21d_read_humidity(uint16_t * humidity)
{
    if (!humidity)
	return false;
    *humidity = htu21d_read_reg16(HTU21D_HOLD_TRIG_HUMI_MEAS);
    return true;
}
