/*
A driver for a DS18B20 thermometer by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf                          (Accessed: 25.03.21)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html     (Accessed: 25.03.21)
*/
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <libopencm3/stm32/gpio.h>

#include "pinmap.h"
#include "log.h"
#include "value.h"
#include "timers.h"

#define DELAY_READ_START    2
#define DELAY_READ_WAIT     10
#define DELAY_READ_RELEASE  50
#define DELAY_WRITE_1_START 6
#define DELAY_WRITE_1_END   64
#define DELAY_WRITE_0_START 60
#define DELAY_WRITE_0_END   10
#define DELAY_RESET_SET     500
#define DELAY_RESET_WAIT    60
#define DELAY_RESET_READ    480
#define DELAY_GET_TEMP      750000

#define W1_CMD_SKIP_ROM     0xCC
#define W1_CMD_CONV_T       0x44
#define W1_CMD_READ_SCP     0xBE

#define W1_PORT             PULSE2_PORT
#define W1_PIN              PULSE2_PIN

#define W1_DIRECTION_OUTPUT true
#define W1_DIRECTION_INPUT  false
#define W1_LEVEL_LOW        (uint8_t)0
#define W1_LEVEL_HIGH       (uint8_t)1

#define W1_DEFAULT_COLLECTION_TIME 1000


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
} w1_memory_t;


static void _w1_start_interrupt(void) {}

static void _w1_stop_interrupt(void) {}


static void _w1_delay_us(uint64_t delay)
{
    timer_delay_us_64(delay);
}


static void _w1_set_direction(bool out)
{
    if (out)
    {
        gpio_mode_setup(W1_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, W1_PIN);
    }
    else
    {
        gpio_mode_setup(W1_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, W1_PIN);
    }
}


static void _w1_set_level(uint8_t bit)
{
    if (bit)
    {
        gpio_set(W1_PORT, W1_PIN);
    }
    else
    {
        gpio_clear(W1_PORT, W1_PIN);
    }
}


static uint8_t _w1_get_level(void)
{
    if (gpio_get(W1_PORT, W1_PIN))
    {
        return W1_LEVEL_HIGH;
    }
    else
    {
        return W1_LEVEL_LOW;
    }
}


static uint8_t _w1_read_bit(void) {
    _w1_start_interrupt(); 
    _w1_set_direction(W1_DIRECTION_OUTPUT);
    _w1_set_level(W1_LEVEL_LOW);
    _w1_delay_us(DELAY_READ_START);
    _w1_set_direction(W1_DIRECTION_INPUT);
    _w1_delay_us(DELAY_READ_WAIT);
    uint8_t out = _w1_get_level();
    _w1_delay_us(DELAY_READ_RELEASE);
    _w1_stop_interrupt();
    return out;
}


static void _w1_send_bit(int bit)
{
    _w1_start_interrupt();
    if (bit)
    {
        _w1_set_level(W1_LEVEL_LOW);
        _w1_delay_us(DELAY_WRITE_1_START);
        _w1_set_level(W1_LEVEL_HIGH);
        _w1_delay_us(DELAY_WRITE_1_END);
    }
    else
    {
        _w1_set_level(W1_LEVEL_LOW);
        _w1_delay_us(DELAY_WRITE_0_START);
        _w1_set_level(W1_LEVEL_HIGH);
        _w1_delay_us(DELAY_WRITE_0_END); 
    }
    _w1_stop_interrupt();
}


static bool _w1_reset(void)
{
    _w1_set_direction(W1_DIRECTION_OUTPUT);
    _w1_set_level(W1_LEVEL_LOW);
    _w1_delay_us(DELAY_RESET_SET);
    _w1_set_direction(W1_DIRECTION_INPUT);
    _w1_delay_us(DELAY_RESET_WAIT);
    if (_w1_get_level() == W1_LEVEL_HIGH)
    {
        return false;
    }
    _w1_delay_us(DELAY_RESET_READ);
    return (_w1_get_level() == W1_LEVEL_HIGH);
}


static uint8_t _w1_read_byte(void)
{
    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        val = val + (_w1_read_bit() << i);
    }
    return val;
}


static void _w1_send_byte(uint8_t byte)
{
    _w1_set_direction(W1_DIRECTION_OUTPUT);
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & (1 << i))
        {
            _w1_send_bit(W1_LEVEL_HIGH);
        }
        else
        {
            _w1_send_bit(W1_LEVEL_LOW);
        }
    }
    _w1_set_direction(W1_DIRECTION_INPUT);
}


static void _w1_read_scpad(w1_memory_t* d)
{
    for (int i = 0; i < 9; i++)
    {
        d->raw[i] = _w1_read_byte();
    }
}


static bool _w1_crc_check(uint8_t* mem, uint8_t size)
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


static void _w1_temp_err(void)
{
    log_debug(DEBUG_IO, "Temperature probe did not respond");
}


bool w1_query_temp(double* temperature)
{
    w1_memory_t d; 
    if (!_w1_reset())
    {
        _w1_temp_err();
        return false;
    }
    _w1_send_byte(W1_CMD_SKIP_ROM);
    _w1_send_byte(W1_CMD_CONV_T);
    _w1_delay_us(DELAY_GET_TEMP);
    if (!_w1_reset())
    {
        _w1_temp_err();
        return false;
    }
    _w1_send_byte(W1_CMD_SKIP_ROM);
    _w1_send_byte(W1_CMD_READ_SCP);
    _w1_read_scpad(&d);

    if (!_w1_crc_check(d.raw, 8))
    {
        log_debug(DEBUG_IO, "Temperature data not confirmed by CRC");
        return false;
    }

    int16_t integer_bits = d.t >> 4;
    int8_t decimal_bits = d.t & 0x000F;
    if (d.t & 0x8000)
    {
        integer_bits = integer_bits | 0xF000;
        decimal_bits = (decimal_bits - 16) % 16;
    }

    *temperature = integer_bits + decimal_bits / 16;
    log_debug(DEBUG_IO, "T: %.03f C", *temperature);
    return true;
}


bool w1_measurement_init(char* name)
{
    if (!_w1_reset())
    {
        _w1_temp_err();
        return false;
    }
    _w1_send_byte(W1_CMD_SKIP_ROM);
    _w1_send_byte(W1_CMD_CONV_T);
    _w1_delay_us(DELAY_GET_TEMP);
    return true;
}


bool w1_measurement_collect(char* name, value_t* value)
{
    w1_memory_t d;
    if (!_w1_reset())
    {
        _w1_temp_err();
        return false;
    }
    _w1_send_byte(W1_CMD_SKIP_ROM);
    _w1_send_byte(W1_CMD_READ_SCP);
    _w1_read_scpad(&d);

    if (!_w1_crc_check(d.raw, 8))
    {
        log_debug(DEBUG_IO, "Temperature data not confirmed by CRC");
        return false;
    }

    int16_t integer_bits = d.t >> 4;
    int8_t decimal_bits = d.t & 0x000F;
    if (d.t & 0x8000)
    {
        integer_bits = integer_bits | 0xF000;
        decimal_bits = (decimal_bits - 16) % 16;
    }

    *value = value_from_float(integer_bits + decimal_bits / 16);
    return true;
}


uint32_t w1_collection_time(void)
{
    return W1_DEFAULT_COLLECTION_TIME;
}


void w1_temp_init(void)
{
    _w1_reset();
}
