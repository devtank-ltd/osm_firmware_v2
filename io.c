#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "io.h"
#include "log.h"
#include "pulsecount.h"
#include "uarts.h"
#include "one_wire_driver.h"

static const port_n_pins_t ios_pins[]           = IOS_PORT_N_PINS;
static uint16_t ios_state[ARRAY_SIZE(ios_pins)] = IOS_STATE;


#define IO_PULL_STR_NONE "NONE"
#define IO_PULL_STR_UP   "UP"
#define IO_PULL_STR_DOWN "DOWN"


static char* _ios_get_pull_str(uint16_t io_state)
{
    switch(io_state & IO_PULL_MASK)
    {
        case GPIO_PUPD_PULLUP:   return IO_PULL_STR_UP;
        case GPIO_PUPD_PULLDOWN: return IO_PULL_STR_DOWN;
        default:
            break;
    }
    return IO_PULL_STR_NONE;
}


static char* _ios_get_type(uint16_t io_state)
{
    switch(io_state & IO_TYPE_MASK)
    {
        case IO_PULSECOUNT: return "PLSCNT";
        case IO_ONEWIRE:    return "W1";
        default : return "";
    }
}


static bool _io_is_special(uint16_t io_state)
{
    switch(io_state & IO_TYPE_MASK)
    {
        case IO_PULSECOUNT:
        case IO_ONEWIRE:
            return true;
        default : return false;
    }
}


static void _ios_setup_gpio(unsigned io, uint16_t io_state)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * gpio_pin = &ios_pins[io];

    gpio_mode_setup(gpio_pin->port,
        (io_state & IO_AS_INPUT)?GPIO_MODE_INPUT:GPIO_MODE_OUTPUT,
        io_state & IO_PULL_MASK,
        gpio_pin->pins);

    ios_state[io] = (ios_state[io] & (IO_TYPE_MASK)) | io_state;

    io_debug("%02u set to %s %s",
            io,
            (io_state & IO_AS_INPUT)?"IN":"OUT",
            _ios_get_pull_str(io_state));
}


void     ios_init()
{
    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(ios_pins[n].port));

        uint16_t io_state = ios_state[n];

        if (io_state & IO_SPECIAL_EN)
            io_debug("%02u : USED %s", n, _ios_get_type(io_state));
        else
            _ios_setup_gpio(n, io_state);
    }
}


unsigned ios_get_count()
{
    return ARRAY_SIZE(ios_pins);
}


void     io_configure(unsigned io, bool as_input, unsigned pull)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    uint16_t io_state = ios_state[io];

    if (io_state & IO_SPECIAL_EN)
    {
        uint16_t io_type = io_state & IO_TYPE_MASK;

        if (io_type == IO_PULSECOUNT)
        {
            pulsecount_enable(false);
            io_state &= ~IO_SPECIAL_EN;
            io_debug("%02u : PLSCNT NO LONGER", io);
        }
        else if (io_type == IO_ONEWIRE)
        {
            w1_enable(false);
            io_state &= ~IO_SPECIAL_EN;
            io_debug("%02u : W1 NO LONGER", io);
        }
    }

    if (io_state & IO_DIR_LOCKED)
    {
        io_debug("GPIO %02u locked but change attempted.", io);
        return;
    }

    if (as_input)
        io_state |= IO_AS_INPUT;
    else
        io_state &= ~IO_AS_INPUT;

    io_state &= ~IO_PULL_MASK;
    io_state |= (pull & IO_PULL_MASK);

    _ios_setup_gpio(io, io_state);
}


bool     io_enable_special(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    uint16_t io_state = ios_state[io];

    if (io_state & IO_SPECIAL_EN)
        return true;

    uint16_t io_type = io_state & IO_TYPE_MASK;

    if (io_type == IO_PULSECOUNT)
    {
        pulsecount_enable(true);
        ios_state[io] |= IO_SPECIAL_EN;
        io_debug("%02u : USED PLSCNT", io);
        return true;
    }
    else if (io_type == IO_ONEWIRE)
    {
        w1_enable(true);
        ios_state[io] |= IO_SPECIAL_EN;
        io_debug("%02u : USED W1", io);
        return true;
    }

    return false;
}


bool     io_is_input(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return (ios_state[io] & IO_AS_INPUT)?true:false;
}


unsigned io_get_bias(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return (ios_state[io] & IO_PULL_MASK);
}


void     io_on(unsigned io, bool on_off)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * output = &ios_pins[io];

    io_debug("%02u = %s", io, (on_off)?"ON":"OFF");

    if (on_off)
        gpio_set(output->port, output->pins);
    else
        gpio_clear(output->port, output->pins);
}


void     io_log(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * gpio_pin = &ios_pins[io];
    uint16_t io_state = ios_state[io];

    char * type = _ios_get_type(io_state);

    if (!(io_state & IO_SPECIAL_EN))
    {
        char * pretype = "";
        char * posttype = "";

        if (type[0])
        {
            if (_io_is_special(io_state))
            {
                pretype = "[";
                posttype = "] ";
            }
            else
            {
                pretype = "";
                posttype = " ";
            }
        }

        if (io_state & IO_AS_INPUT)
            log_out("IO %02u : %s%s%sIN %s = %s",
                    io, pretype, type, posttype,
                    _ios_get_pull_str(io_state),
                    (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
        else
            log_out("IO %02u : %s%s%sOUT %s = %s",
                    io, pretype, type, posttype,
                    _ios_get_pull_str(io_state),
                    (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
    }
    else log_out("IO %02u : USED %s", io, type);
}


void     ios_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
        io_log(n);
}
