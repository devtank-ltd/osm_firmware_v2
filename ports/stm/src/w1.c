#include <stdbool.h>
#include <inttypes.h>

#include <libopencm3/stm32/gpio.h>

#include "w1.h"

#include "timers.h"
#include "pinmap.h"
#include "log.h"
#include "base_types.h"
#include "io.h"
#include "version.h"


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


typedef struct
{
    port_n_pins_t   pnp;
    unsigned        io;
    port_n_pins_t   pupd_pnp;
} w1_ios_t;


static w1_ios_t _w1_ios[] = { {.pnp={ W1_PULSE_PORT, W1_PULSE_1_PIN }, .io=W1_PULSE_1_IO, .pupd_pnp={W1_PULSE_1_PULLUP_EN_PORT, W1_PULSE_1_PULLUP_EN_PIN}} };

static void _w1_start_interrupt(void) {}

static void _w1_stop_interrupt(void) {}


static bool _w1_check_index(uint8_t index)
{
    if (index >= ARRAY_SIZE(_w1_ios))
    {
        log_error("W1 index references uninitialised memory.");
        return false;
    }
    if (!io_is_w1_now(_w1_ios[index].io))
    {
        log_error("IO not set up for W1.");
        return false;
    }
    return true;
}


static void _w1_delay_us(uint64_t delay)
{
    timer_delay_us_64(delay);
}


static void _w1_set_direction(uint8_t index, bool out)
{
    if (!_w1_check_index(index))
        return;

    if (out)
    {
        gpio_mode_setup(_w1_ios[index].pnp.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, _w1_ios[index].pnp.pins);
    }
    else
    {
        gpio_mode_setup(_w1_ios[index].pnp.port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, _w1_ios[index].pnp.pins);
    }
}


static void _w1_set_level(uint8_t index, uint8_t bit)
{
    if (!_w1_check_index(index))
        return;

    if (bit)
    {
        gpio_set(_w1_ios[index].pnp.port, _w1_ios[index].pnp.pins);
    }
    else
    {
        gpio_clear(_w1_ios[index].pnp.port, _w1_ios[index].pnp.pins);
    }
}


static uint8_t _w1_get_level(uint8_t index)
{
    if (!_w1_check_index(index))
        return 0;

    if (gpio_get(_w1_ios[index].pnp.port, _w1_ios[index].pnp.pins))
    {
        return W1_LEVEL_HIGH;
    }
    else
    {
        return W1_LEVEL_LOW;
    }
}


static uint8_t _w1_read_bit(uint8_t index)
{
    if (!_w1_check_index(index))
        return 0;

    _w1_start_interrupt(); 
    _w1_set_direction(index, W1_DIRECTION_OUTPUT);
    _w1_set_level(index, W1_LEVEL_LOW);
    _w1_delay_us(W1_DELAY_READ_START);
    _w1_set_direction(index, W1_DIRECTION_INPUT);
    _w1_delay_us(W1_DELAY_READ_WAIT);
    uint8_t out = _w1_get_level(index);
    _w1_delay_us(W1_DELAY_READ_RELEASE);
    _w1_stop_interrupt();
    return out;
}


static void _w1_send_bit(uint8_t index, int bit)
{
    if (!_w1_check_index(index))
        return;

    _w1_start_interrupt();
    if (bit)
    {
        _w1_set_level(index, W1_LEVEL_LOW);
        _w1_delay_us(W1_DELAY_WRITE_1_START);
        _w1_set_level(index, W1_LEVEL_HIGH);
        _w1_delay_us(W1_DELAY_WRITE_1_END);
    }
    else
    {
        _w1_set_level(index, W1_LEVEL_LOW);
        _w1_delay_us(W1_DELAY_WRITE_0_START);
        _w1_set_level(index, W1_LEVEL_HIGH);
        _w1_delay_us(W1_DELAY_WRITE_0_END); 
    }
    _w1_stop_interrupt();
}


bool w1_reset(uint8_t index)
{
    if (!_w1_check_index(index))
        return 0;


    _w1_set_direction(index, W1_DIRECTION_OUTPUT);
    _w1_set_level(index, W1_LEVEL_LOW);
    _w1_delay_us(W1_DELAY_RESET_SET);
    _w1_set_direction(index, W1_DIRECTION_INPUT);
    _w1_delay_us(W1_DELAY_RESET_WAIT);
    if (_w1_get_level(index) == W1_LEVEL_HIGH)
    {
        return false;
    }
    _w1_delay_us(W1_DELAY_RESET_READ);
    return (_w1_get_level(index) == W1_LEVEL_HIGH);
}


uint8_t w1_read_byte(uint8_t index)
{
    if (!_w1_check_index(index))
        return 0;

    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        val = val + (_w1_read_bit(index) << i);
    }
    return val;
}


void w1_send_byte(uint8_t index, uint8_t byte)
{
    if (!_w1_check_index(index))
        return;

    _w1_set_direction(index, W1_DIRECTION_OUTPUT);
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & (1 << i))
        {
            _w1_send_bit(index, W1_LEVEL_HIGH);
        }
        else
        {
            _w1_send_bit(index, W1_LEVEL_LOW);
        }
    }
    _w1_set_direction(index, W1_DIRECTION_INPUT);
}

void w1_init(uint8_t index)
{
    if (index >= ARRAY_SIZE(_w1_ios))
    {
        log_error("Tried to init w1 from uninitialised memory.");
        return;
    }
    rcc_periph_clock_enable(PORT_TO_RCC(_w1_ios[index].pnp.port));

    if (version_is_arch(VERSION_ARCH_REV_C))
    {
        _w1_ios[0].pupd_pnp.pins = W1_PULSE_1_PULLUP_EN_PIN;
        _w1_ios[0].pupd_pnp.port = W1_PULSE_1_PULLUP_EN_PIN;
    }
}


static void _w1_enable_pupd(uint8_t index, bool enabled)
{
    if (index > ARRAY_SIZE(_w1_ios))
        return;

    w1_ios_t* w1_io = &_w1_ios[index];
    if (version_is_arch(VERSION_ARCH_REV_C))
    {
        rcc_periph_clock_enable(w1_io->pupd_pnp.port);
        gpio_mode_setup(w1_io->pupd_pnp.port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, w1_io->pupd_pnp.pins);
        if (enabled)
            gpio_set(w1_io->pupd_pnp.port, w1_io->pupd_pnp.pins);
        else
            gpio_clear(w1_io->pupd_pnp.port, w1_io->pupd_pnp.pins);
    }
}


void w1_enable(unsigned io, bool enabled)
{
    for (unsigned index = 0; index < ARRAY_SIZE(_w1_ios); index++)
    {
        if (_w1_ios[index].io == io)
        {
            _w1_enable_pupd(index, enabled);
            return ;
        }
    }
}
