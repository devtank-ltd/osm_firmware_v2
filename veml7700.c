/*
Datasheet: https://eu.mouser.com/datasheet/2/427/VISH_S_A0012091125_1-2572303.pdf
         (Accessed: 31.01.2022)
*/


#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "i2c.h"
#include "log.h"
#include "measurements.h"
#include "sys_time.h"
#include "veml7700.h"
#include "uart_rings.h"
#include "timers.h"


#define VEML7700_WAIT_MEASUREMENT_MS            110
#define VEML7700_RES_SCALE                      10000
#define VEML7700_COUNT_LOWER_THRESHOLD          100
#define VEML7700_COUNT_UPPER_THRESHOLD          10000


typedef enum
{
    VEML7700_CMD_ALS_CONF_0     = 0x00,
    VEML7700_CMD_ALS_WH         = 0x01,
    VEML7700_CMD_ALS_WL         = 0x02,
    VEML7700_CMD_POWER_SAVING   = 0x03,
    VEML7700_CMD_ALS            = 0x04,
    VEML7700_CMD_WHITE          = 0x05,
    VEML7700_CMD_ALS_INT        = 0x06,
} veml7700_cmd_t;


typedef enum
{
    VEML7700_CONF_ALS_SM_GAIN_1   = 0 , /* 0b00 */
    VEML7700_CONF_ALS_SM_GAIN_2   = 1 , /* 0b01 */
    VEML7700_CONF_ALS_SM_GAIN_1_8 = 2 , /* 0b10 */
    VEML7700_CONF_ALS_SM_GAIN_1_4 = 3 , /* 0b11 */
} veml7700_als_gains_t;


typedef enum
{
    VEML7700_CONF_ALS_IT_25  = 0xC , /* 0b1100 */
    VEML7700_CONF_ALS_IT_50  = 0x8 , /* 0b1000 */
    VEML7700_CONF_ALS_IT_100 = 0x0 , /* 0b0000 */
    VEML7700_CONF_ALS_IT_200 = 0x1 , /* 0b0001 */
    VEML7700_CONF_ALS_IT_400 = 0x2 , /* 0b0010 */
    VEML7700_CONF_ALS_IT_800 = 0x3 , /* 0b0011 */
} veml7700_als_int_times_t;


typedef enum
{
    VEML7700_CONF_ALS_PERS_1 = 0, /* 0b00 */
    VEML7700_CONF_ALS_PERS_2 = 1, /* 0b01 */
    VEML7700_CONF_ALS_PERS_4 = 2, /* 0b10 */
    VEML7700_CONF_ALS_PERS_8 = 3, /* 0b11 */
} veml7700_als_persitent_t;

typedef enum
{
    VEML7700_CONF_ALS_INT_EN_DIABLED = 0 ,
    VEML7700_CONF_ALS_INT_EN_ENABLED = 1 ,
} veml7700_als_int_en_t;


typedef enum
{
    VEML7700_CONF_ALS_SD_ON  = 0 ,
    VEML7700_CONF_ALS_SD_OFF = 1 ,
} veml7700_als_shutdown_t;


typedef enum
{
    VEML7700_PWR_PSM_MODE_1 = 0 , /* 0b00 */
    VEML7700_PWR_PSM_MODE_2 = 1 , /* 0b01 */
    VEML7700_PWR_PSM_MODE_3 = 2 , /* 0b10 */
    VEML7700_PWR_PSM_MODE_4 = 3 , /* 0b11 */
} veml7700_pwr_psm_t;


typedef enum
{
    VEML7700_PWR_PSM_EN_DISABLE = 0 ,
    VEML7700_PWR_PSM_EN_ENABLE  = 1 ,
} veml7700_pwr_en_t;


typedef union
{
    struct
    {
        uint16_t als_sd:1;
        uint16_t als_int_en:1;
        uint16_t _:2;
        uint16_t als_pers:2;
        uint16_t als_it:4;
        uint16_t __:1;
        uint16_t als_sm:2;
        uint16_t ___:3;
    };
    uint16_t raw;
} __attribute__((__packed__)) veml7700_cmd_conf_t;


typedef union
{
    struct
    {
        uint16_t psm_en:1;
        uint16_t psm:2;
        uint16_t _:13;
    };
    uint16_t raw;
} __attribute__((__packed__)) veml7700_cmd_pwr_t;


typedef struct
{
    veml7700_cmd_conf_t     config;
    veml7700_cmd_pwr_t      power;
    uint32_t                refresh_time;
    uint16_t                resolution_scaled; // scaled by VEML7700_RES_SCALE
} veml7700_conf_t;


static veml7700_conf_t _veml7700_conf = {.config={.als_sd     = VEML7700_CONF_ALS_SD_OFF,
                                                  .als_int_en = VEML7700_CONF_ALS_INT_EN_DIABLED,
                                                  .als_pers   = VEML7700_CONF_ALS_PERS_1,
                                                  .als_it     = VEML7700_CONF_ALS_IT_100,
                                                  .als_sm     = VEML7700_CONF_ALS_SM_GAIN_1_4},
                                         .power={.psm_en = VEML7700_PWR_PSM_EN_DISABLE,
                                                 .psm    = VEML7700_PWR_PSM_MODE_1}};


static void _veml7700_get_u16(uint8_t d[2], uint16_t *r)
{
    *r = (d[1] << 8) | d[0];
    light_debug("Got [0x%"PRIx8", 0x%"PRIx8"] = %"PRIu16, d[0], d[1], *r);
}


static bool _veml7700_read_reg16(veml7700_cmd_t reg, uint16_t * r)
{
    if (!r)
    {
        return false;
    }
    uint8_t reg8 = reg;
    uint8_t d[2] = {0};
    light_debug("Read command 0x%"PRIx8, reg8);
    i2c_transfer7(VEML7700_I2C, I2C_VEML7700_ADDR, &reg8, 1, d, 2);
    _veml7700_get_u16(d, r);
    return true;
}


static void _veml7700_write_reg16(veml7700_cmd_t reg, uint16_t data)
{
    uint8_t payload[3] = { reg, data & 0xFF, data >> 8 };
    light_debug("Send command 0x%"PRIx8" [0x%"PRIx8" 0x%"PRIx8"].", payload[0], payload[1], payload[2]);
    i2c_transfer7(VEML7700_I2C, I2C_VEML7700_ADDR, payload, 3, NULL, 0);
}


static uint16_t _veml7700_get_resolution(void)
{
    uint16_t resolution_scaled = 0.0036 * VEML7700_RES_SCALE;
    uint8_t multiplier = 1;
    switch ((veml7700_als_gains_t)_veml7700_conf.config.als_sm)
    {
        case VEML7700_CONF_ALS_SM_GAIN_1:
            multiplier *= 2;
            break;
        case VEML7700_CONF_ALS_SM_GAIN_2:
            multiplier *= 1;
            break;
        case VEML7700_CONF_ALS_SM_GAIN_1_4:
            multiplier *= 8;
            break;
        case VEML7700_CONF_ALS_SM_GAIN_1_8:
            multiplier *= 16;
            break;
    }
    switch ((veml7700_als_int_times_t)_veml7700_conf.config.als_it)
    {
        case VEML7700_CONF_ALS_IT_25:
            multiplier *= 32;
            break;
        case VEML7700_CONF_ALS_IT_50:
            multiplier *= 16;
            break;
        case VEML7700_CONF_ALS_IT_100:
            multiplier *= 8;
            break;
        case VEML7700_CONF_ALS_IT_200:
            multiplier *= 4;
            break;
        case VEML7700_CONF_ALS_IT_400:
            multiplier *= 2;
            break;
        case VEML7700_CONF_ALS_IT_800:
            multiplier *= 1;
            break;
    }
    return resolution_scaled * multiplier;
}


static uint32_t _veml7700_get_refresh_time(void)
{
    uint32_t refresh_time = 300;
    uint32_t offset = 0;
    switch ((veml7700_pwr_psm_t)_veml7700_conf.power.psm)
    {
        case VEML7700_PWR_PSM_MODE_1:
            offset += 0;
            break;
        case VEML7700_PWR_PSM_MODE_2:
            offset += 500;
            break;
        case VEML7700_PWR_PSM_MODE_3:
            offset += 1500;
            break;
        case VEML7700_PWR_PSM_MODE_4:
            offset += 3500;
            break;
    }
    switch ((veml7700_als_int_times_t)_veml7700_conf.config.als_it)
    {
        case VEML7700_CONF_ALS_IT_25:
            offset += 0;
            break;
        case VEML7700_CONF_ALS_IT_50:
            offset += 150;
            break;
        case VEML7700_CONF_ALS_IT_100:
            offset += 300;
            break;
        case VEML7700_CONF_ALS_IT_200:
            offset += 500;
            break;
        case VEML7700_CONF_ALS_IT_400:
            offset += 700;
            break;
        case VEML7700_CONF_ALS_IT_800:
            offset += 1100;
            break;
    }
    return refresh_time + offset;
}


static void _veml7700_set_config(void)
{
    _veml7700_write_reg16(VEML7700_CMD_POWER_SAVING, _veml7700_conf.power.raw);
    _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, _veml7700_conf.config.raw);
    _veml7700_conf.resolution_scaled = _veml7700_get_resolution();
    _veml7700_conf.refresh_time = _veml7700_get_refresh_time();
}


static void _veml7700_turn_on(void)
{
    _veml7700_conf.config.als_sd = VEML7700_CONF_ALS_SD_ON;
    _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, _veml7700_conf.config.raw);
}


static void _veml7700_turn_off(void)
{
    _veml7700_conf.config.als_sd = VEML7700_CONF_ALS_SD_OFF;
    _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, _veml7700_conf.config.raw);
}


static uint16_t _veml7700_read_als(void)
{
    uint16_t als_data;
    _veml7700_read_reg16(VEML7700_CMD_ALS, &als_data);
    return als_data;
}


static bool _veml7700_dt_correction(uint32_t* lux_dt, uint32_t lux)
{
    /**
     Coeffients calculated with a OSM board and cover.
     lux_dt = Ax^2 + Bx
        where x = lux
     */
    uint32_t inter_val = lux;
    const float A = +3.2018E-04f;
    const float B = +4.3223E+00f;
    inter_val =   (A * lux * lux)
                + (B * lux);
    *lux_dt = (uint32_t)inter_val;
    return true;
}


static bool _veml7700_conv_lux(uint32_t* lux_corrected, uint16_t counts)
{
    /**
     lux = counts * resolution
     lux_corrected = Ax^4 + Bx^3 + Cx^2 + Dx
           where x = lux
     */
    uint64_t lux = (counts * _veml7700_conf.resolution_scaled) / VEML7700_RES_SCALE;
    light_debug("Lux before correction = %"PRIu64, lux);
    const float A = +6.0135E-13f;
    const float B = -9.3924E-09f;
    const float C = +8.1488E-05f;
    const float D = +1.0023E+00f;
    lux =   (A * lux * lux * lux * lux)
          + (B * lux * lux * lux)
          + (C * lux * lux)
          + (D * lux);
    light_debug("Lux after correction = %"PRIu64, lux);
    if (lux > UINT32_MAX)
    {
        light_debug("Cannot downsize the lux.");
        return false;
    }
    *lux_corrected = (uint32_t)lux;
    return true;
}


bool veml7700_get_lux(uint32_t* lux)
{
    if (!lux)
    {
        light_debug("Handed in null pointer.");
        return false;
    }
    uint32_t lux_local;
    _veml7700_set_config();
    _veml7700_turn_on();
    uint32_t start_ms = since_boot_ms;
    light_debug("Waiting %"PRIu16".%03"PRIu16" seconds", VEML7700_WAIT_MEASUREMENT_MS/1000, VEML7700_WAIT_MEASUREMENT_MS%1000);
    while (since_boot_delta(since_boot_ms, start_ms) < VEML7700_WAIT_MEASUREMENT_MS)
        uart_rings_out_drain();

    uint16_t counts = _veml7700_read_als();
    _veml7700_turn_off();
    light_debug("Raw light count = %"PRIu16, counts);
    if (!_veml7700_conv_lux(&lux_local, counts))
    {
        return false;
    }
    return _veml7700_dt_correction(lux, lux_local);
}


uint32_t veml7700_measurements_collection_time(void)
{
    return VEML7700_WAIT_MEASUREMENT_MS;
}


bool veml7700_light_measurements_init(char* name)
{
    _veml7700_set_config();
    _veml7700_turn_on();
    return true;
}


bool veml7700_light_measurements_get(char* name, value_t* value)
{
    uint16_t counts;
    uint32_t lux;
    counts = _veml7700_read_als();
    _veml7700_turn_off();
    if (!_veml7700_conv_lux(&lux, counts))
    {
        return false;
    }
    if (!_veml7700_dt_correction(&lux, lux))
    {
        return false;
    }
    *value = value_from_u32(lux);
    return true;
}


void veml7700_init(void)
{
    i2c_init(VEML7700_I2C_INDEX);
}
