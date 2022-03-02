#include <stdbool.h>
#include <inttypes.h>

#include <libopencm3/stm32/gpio.h>

#include "timers.h"
#include "io.h"


#define W1_DELAY_READ_START    2
#define W1_DELAY_READ_WAIT     10
#define W1_DELAY_READ_RELEASE  50
#define W1_DELAY_WRITE_1_START 6
#define W1_DELAY_WRITE_1_END   64
#define W1_DELAY_WRITE_0_START 60
#define W1_DELAY_WRITE_0_END   10
#define W1_DELAY_RESET_SET     500
#define W1_DELAY_RESET_WAIT    60
#define W1_DELAY_RESET_READ    480

#define W1_DIRECTION_OUTPUT true
#define W1_DIRECTION_INPUT  false
#define W1_LEVEL_LOW        (uint8_t)0
#define W1_LEVEL_HIGH       (uint8_t)1


static void _w1_start_interrupt(void) {}

static void _w1_stop_interrupt(void) {}


static void _w1_delay_us(uint64_t delay)
{
    timer_delay_us_64(delay);
}


static void _w1_set_direction(uint8_t w1_port, uint8_t w1_pin, bool out)
{
    if (out)
    {
        gpio_mode_setup(w1_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, w1_pin);
    }
    else
    {
        gpio_mode_setup(w1_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, w1_pin);
    }
}


static void _w1_set_level(uint8_t w1_port, uint8_t w1_pin, uint8_t bit)
{
    if (bit)
    {
        gpio_set(w1_port, w1_pin);
    }
    else
    {
        gpio_clear(w1_port, w1_pin);
    }
}


static uint8_t _w1_get_level(uint8_t w1_port, uint8_t w1_pin)
{
    if (gpio_get(w1_port, w1_pin))
    {
        return W1_LEVEL_HIGH;
    }
    else
    {
        return W1_LEVEL_LOW;
    }
}


static uint8_t _w1_read_bit(uint8_t w1_port, uint8_t w1_pin)
{
    _w1_start_interrupt(); 
    _w1_set_direction(w1_port, w1_pin, W1_DIRECTION_OUTPUT);
    _w1_set_level(w1_port, w1_pin, W1_LEVEL_LOW);
    _w1_delay_us(W1_DELAY_READ_START);
    _w1_set_direction(w1_port, w1_pin, W1_DIRECTION_INPUT);
    _w1_delay_us(W1_DELAY_READ_WAIT);
    uint8_t out = _w1_get_level(w1_port, w1_pin);
    _w1_delay_us(W1_DELAY_READ_RELEASE);
    _w1_stop_interrupt();
    return out;
}


static void _w1_send_bit(uint8_t w1_port, uint8_t w1_pin, int bit)
{
    _w1_start_interrupt();
    if (bit)
    {
        _w1_set_level(w1_port, w1_pin, W1_LEVEL_LOW);
        _w1_delay_us(W1_DELAY_WRITE_1_START);
        _w1_set_level(w1_port, w1_pin, W1_LEVEL_HIGH);
        _w1_delay_us(W1_DELAY_WRITE_1_END);
    }
    else
    {
        _w1_set_level(w1_port, w1_pin, W1_LEVEL_LOW);
        _w1_delay_us(W1_DELAY_WRITE_0_START);
        _w1_set_level(w1_port, w1_pin, W1_LEVEL_HIGH);
        _w1_delay_us(W1_DELAY_WRITE_0_END); 
    }
    _w1_stop_interrupt();
}


bool w1_reset(uint8_t w1_port, uint8_t w1_pin)
{
    _w1_set_direction(w1_port, w1_pin, W1_DIRECTION_OUTPUT);
    _w1_set_level(w1_port, w1_pin, W1_LEVEL_LOW);
    _w1_delay_us(W1_DELAY_RESET_SET);
    _w1_set_direction(w1_port, w1_pin, W1_DIRECTION_INPUT);
    _w1_delay_us(W1_DELAY_RESET_WAIT);
    if (_w1_get_level(w1_port, w1_pin) == W1_LEVEL_HIGH)
    {
        return false;
    }
    _w1_delay_us(W1_DELAY_RESET_READ);
    return (_w1_get_level(w1_port, w1_pin) == W1_LEVEL_HIGH);
}


uint8_t w1_read_byte(uint8_t w1_port, uint8_t w1_pin)
{
    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        val = val + (_w1_read_bit(w1_port, w1_pin) << i);
    }
    return val;
}


void w1_send_byte(uint8_t w1_port, uint8_t w1_pin, uint8_t byte)
{
    _w1_set_direction(w1_port, w1_pin, W1_DIRECTION_OUTPUT);
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & (1 << i))
        {
            _w1_send_bit(w1_port, w1_pin, W1_LEVEL_HIGH);
        }
        else
        {
            _w1_send_bit(w1_port, w1_pin, W1_LEVEL_LOW);
        }
    }
    _w1_set_direction(w1_port, w1_pin, W1_DIRECTION_INPUT);
}
