#include <stdint.h>
#include <string.h>

#include "measurements.h"
#include "i2c.h"
#include "pinmap.h"
#include "common.h"
#include "log.h"

#define TMP4718_ADDR                                0x4C /* 0x4D for TMP4718BDGKR */
#define TMP4718_TIMEOUT_MS                          10
#define TMP4718_MEASUREMENT_COLLECTION_TIME_MS      1000

/*      REGISTER NAME                               ADDR   ALT_ADDR     TYPE        RESET  */
#define TMP4718_REG_ADDR_TEMP_LOCAL                 0x00                /* R  */    /* 00h */
#define TMP4718_REG_ADDR_TEMP_REMOTE_MSB            0x01                /* R  */    /* 00h */
#define TMP4718_REG_ADDR_ALERT_STATUS               0x02                /* RC */    /* 00h */
#define TMP4718_REG_ADDR_CONFIGURATION              0x03    /* 09h */   /* RW */    /* 05h */
#define TMP4718_REG_ADDR_CONV_PERIOD                0x04    /* 0Ah */   /* RW */    /* 08h */
#define TMP4718_REG_ADDR_THIGH_LIMIT_LOCAL          0x05    /* 0Bh */   /* RW */    /* 46h */
#define TMP4718_REG_ADDR_THIGH_LIMIT_REMOTE_MSB     0x07    /* 0Dh */   /* RW */    /* 46h */
#define TMP4718_REG_ADDR_TLOW_LIMIT_REMOTE_MSB      0x08    /* 0Eh */   /* RW */    /* D8h */
#define TMP4718_REG_ADDR_ONE_SHOT                   0x0F                /*  W */    /* 00h */
#define TMP4718_REG_ADDR_TEMP_REMOTE_LSB            0x10                /* R  */    /* 00h */
#define TMP4718_REG_ADDR_REMOTE_OFFSET_MSB          0x11                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_REMOTE_OFFSET_LSB          0x12                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_THIGH_LIMIT_REMOTE_LSB     0x13                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_TLOW_LIMIT_REMOTE_LSB      0x14                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_ALERT_MASK                 0x16                /* RW */    /* 07h */
#define TMP4718_REG_ADDR_THIGH_CRIT_REMOTE          0x19                /* RW */    /* XXh */
#define TMP4718_REG_ADDR_THIGH_CRIT_LOCAL           0x20                /* RW */    /* XXh */
#define TMP4718_REG_ADDR_CRIT_HYSTERESIS            0x21                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_LOG1                       0x2D                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_LOG2                       0x2E                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_LOG3                       0x2F                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_FILTER_ALERT_MODE          0xBF                /* RW */    /* 00h */
#define TMP4718_REG_ADDR_CHIP_ID                    0xFD                /* R  */    /* 50h */
#define TMP4718_REG_ADDR_VENDOR_ID                  0xFE                /* R  */    /* 60h */
#define TMP4718_REG_ADDR_DEVICE_REV_ID              0xFF                /* R  */    /* 90h */

#define TMP4718_REG_ALERT_STATUS_ADC_BUSY           (1 << 7)
#define TMP4718_REG_ALERT_STATUS_THIGH_LA           (1 << 6)
#define TMP4718_REG_ALERT_STATUS_THIGH_RA           (1 << 4)
#define TMP4718_REG_ALERT_STATUS_TLOW_RA            (1 << 3)
#define TMP4718_REG_ALERT_STATUS_REMOTE_DC          (1 << 2)
#define TMP4718_REG_ALERT_STATUS_TCRIT_R            (1 << 1)
#define TMP4718_REG_ALERT_STATUS_TCRIT_L            (1 << 0)

#define TMP4718_REG_CONFIGURATION_ALERT_MASK        (1 << 7)
#define TMP4718_REG_CONFIGURATION_MODE              (1 << 6)
#define TMP4718_REG_CONFIGURATION_REMOTE_EN         (1 << 2)
#define TMP4718_REG_CONFIGURATION_WTC_EN            (1 << 1)
#define TMP4718_REG_CONFIGURATION_FAULT_Q           (1 << 0)

#define TMP4718_REG_CONV_PERIOD_16S                 00
#define TMP4718_REG_CONV_PERIOD_8S                  01
#define TMP4718_REG_CONV_PERIOD_4S                  02
#define TMP4718_REG_CONV_PERIOD_2S                  03
#define TMP4718_REG_CONV_PERIOD_1S                  04
#define TMP4718_REG_CONV_PERIOD_0_5S                05
#define TMP4718_REG_CONV_PERIOD_0_25S               06
#define TMP4718_REG_CONV_PERIOD_0_125S              07
#define TMP4718_REG_CONV_PERIOD_0_0625S             08 /* default */

#define TMP4718_REV_ID_MASK                         0x0F
#define TMP4718_DEVICE_ID_MASK                      0xF0
#define TMP4718_REV_ID(_x)                          (_x && TMP4718_REV_ID_MASK)
#define TMP4718_DEVICE_ID(_x)                       ((_x && TMP4718_DEVICE_ID_MASK) >> 4)

#define __TMP4718_READ_REG(_addr, _val)             \
    if (!_val)                                      \
    {                                               \
        return false;                               \
    }                                               \
    return _tmp4718_read_reg(_addr, _val)


typedef enum
{
    TMP4718_MEASUREMENT_NONE,
    TMP4718_MEASUREMENT_LOCAL,
    TMP4718_MEASUREMENT_REMOTE,
    TMP4718_MEASUREMENT_COUNT,
} tmp4718_measurement_t;


typedef union
{
    uint16_t d;
    struct
    {
        uint8_t l;
        uint8_t h;
    };
} tmp4718_8h_8l_t;


static bool _tmp4718_read_reg(uint8_t addr, uint8_t* value)
{
    return i2c_transfer_timeout(TMP4718_I2C, TMP4718_ADDR, &addr, 1, value, 1, TMP4718_TIMEOUT_MS);
}


static float _tmp4718_remote_conv(uint16_t temp16)
{
    return (float)temp16 / 256.f;
}


static bool _tmp4718_read_local_temperature(uint8_t* val)
{
    exttemp_debug("READING LOCAL TEMPERATURE");
    __TMP4718_READ_REG(TMP4718_REG_ADDR_TEMP_LOCAL, val);
}


static bool _tmp4718_read_remote_temperature(float* val)
{
    exttemp_debug("READING REMOTE TEMPERATURE");
    if (!val)
    {
        return false;
    }
    tmp4718_8h_8l_t temp16 = {0};
    if (!_tmp4718_read_reg(TMP4718_REG_ADDR_TEMP_REMOTE_MSB, &temp16.h))
    {
        return false;
    }
    if (!_tmp4718_read_reg(TMP4718_REG_ADDR_TEMP_REMOTE_LSB, &temp16.l))
    {
        return false;
    }
    *val = _tmp4718_remote_conv(temp16.d);
    return true;
}


void tmp4718_init(void)
{
    i2cs_init();
}


static bool _tmp4718_is_ref(char* name, const char* ref)
{
    unsigned len = strlen(name);
    unsigned reflen = strlen(ref);
    return (reflen == len && strncmp(name, ref, reflen) == 0);
}


static tmp4718_measurement_t _tmp4718_name_to_enum(char* name)
{
    if (_tmp4718_is_ref(name, MEASUREMENTS_TMP4718_LOCAL_NAME))
    {
        return TMP4718_MEASUREMENT_LOCAL;
    }
    else if (_tmp4718_is_ref(name, MEASUREMENTS_TMP4718_REMOTE_NAME))
    {
        return TMP4718_MEASUREMENT_REMOTE;
    }
    return TMP4718_MEASUREMENT_NONE;
}


static measurements_value_type_t _tmp4718_value_type(char* name)
{
    tmp4718_measurement_t meas = _tmp4718_name_to_enum(name);
    switch (meas)
    {
        case TMP4718_MEASUREMENT_REMOTE:
            return MEASUREMENTS_VALUE_TYPE_FLOAT;
        case TMP4718_MEASUREMENT_LOCAL:
            return MEASUREMENTS_VALUE_TYPE_I64;
        default:
            break;
    }
    return MEASUREMENTS_VALUE_TYPE_I64;
}


static measurements_sensor_state_t _tmp4718_collect(char* name, measurements_reading_t* value)
{
    tmp4718_measurement_t meas = _tmp4718_name_to_enum(name);
    switch (meas)
    {
        case TMP4718_MEASUREMENT_LOCAL:
        {
            uint8_t temp8 = 0;
            if (!_tmp4718_read_local_temperature(&temp8))
            {
                return MEASUREMENTS_SENSOR_STATE_ERROR;
            }
            value->v_i64 = (int64_t)temp8;
            break;
        }
        case TMP4718_MEASUREMENT_REMOTE:
        {
            float tempf = 0.f;
            if (!_tmp4718_read_remote_temperature(&tempf))
            {
                return MEASUREMENTS_SENSOR_STATE_ERROR;
            }
            value->v_f32 = to_f32_from_float(tempf);
            break;
        }
        default:
            return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void tmp4718_inf_init(measurements_inf_t* inf)
{
    inf->get_cb             = _tmp4718_collect;
    inf->value_type_cb      = _tmp4718_value_type;
}
