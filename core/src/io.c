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
#include "persist_config.h"

static const port_n_pins_t ios_pins[]           = IOS_PORT_N_PINS;
static uint16_t * ios_state;


static char* _ios_get_type(uint16_t io_state)
{
    switch(io_state & IO_TYPE_MASK)
    {
        case IO_TYPE_PULSECOUNT: return "PLSCNT";
        case IO_TYPE_ONEWIRE:    return "W1";
        default : return "";
    }
}


static void _ios_setup_gpio(unsigned io, uint16_t io_state)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    char * type = _ios_get_type(io_state);

    const port_n_pins_t * gpio_pin = &ios_pins[io];

    gpio_mode_setup(gpio_pin->port,
        (io_state & IO_AS_INPUT)?GPIO_MODE_INPUT:GPIO_MODE_OUTPUT,
        io_state & IO_PULL_MASK,
        gpio_pin->pins);

    ios_state[io] = (ios_state[io] & (IO_TYPE_MASK)) | io_state;

    io_debug("%02u set to %s %s%s%s%s",
            io,
            (io_state & IO_AS_INPUT)?"IN":"OUT",
            io_get_pull_str(io_state),
            (type[0])?" [":"",type,(type[0])?"]":"");
}


void     ios_init(void)
{
    ios_state = persist_get_ios_state();

    if (ios_state[0] == 0 || ios_state[0] == 0xFFFF)
    {
        io_debug("No IOs state, setting to default.");
        uint16_t ios_init_state[ARRAY_SIZE(ios_pins)] = IOS_STATE;
        memcpy(ios_state, ios_init_state, sizeof(ios_init_state));
    }

    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(ios_pins[n].port));

        uint16_t io_state = ios_state[n];

        if (io_state & IO_PULSE || io_state & IO_ONEWIRE)
            io_debug("%02u : USED %s", n, _ios_get_type(io_state));
        else
            _ios_setup_gpio(n, io_state);
    }
}


unsigned ios_get_count(void)
{
    return ARRAY_SIZE(ios_pins);
}


bool io_enable_w1(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    if (!(ios_state[io] & IO_TYPE_ONEWIRE))
        return false;

    ios_state[io] &= ~IO_PULSE;
    ios_state[io] &= ~IO_OUT_ON;

    ios_state[io] |= IO_ONEWIRE;
    w1_enable(true);
    io_debug("%02u : USED W1", io);
    return true;
}


bool io_enable_pulsecount(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    if (!(ios_state[io] & IO_TYPE_PULSECOUNT))
        return false;

    ios_state[io] &= ~IO_ONEWIRE;
    ios_state[io] &= ~IO_OUT_ON;

    ios_state[io] |= IO_PULSE;
    pulsecount_enable(true);
    io_debug("%02u : USED PLSCNT", io);
    return true;
}


void     io_configure(unsigned io, bool as_input, unsigned pull)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    uint16_t io_state = ios_state[io];
    uint16_t io_type = io_state & IO_TYPE_MASK;

    if (io_state & IO_PULSE)
    {
        if (!(io_type & IO_TYPE_PULSECOUNT))
        {
            // Error?
            return;
        }
        ios_state[io] &= ~IO_PULSE;
        pulsecount_enable(false);
        io_debug("%02u : PLSCNT NO LONGER", io);
        return;
    }

    else if (io_state & IO_ONEWIRE)
    {
        if (!(io_type & IO_TYPE_ONEWIRE))
        {
            // Error?
            return;
        }
        ios_state[io] &= ~IO_ONEWIRE;
        w1_enable(false);
        io_debug("%02u : W1 NO LONGER", io);
        return;
    }

    if (io_state & IO_DIR_LOCKED)
    {
        io_debug("GPIO %02u locked but change attempted.", io);
        return;
    }

    io_state &= ~IO_OUT_ON;

    if (as_input)
        io_state |= IO_AS_INPUT;
    else
        io_state &= ~IO_AS_INPUT;

    io_state &= ~IO_PULL_MASK;
    io_state |= (pull & IO_PULL_MASK);

    _ios_setup_gpio(io, io_state);
}


bool io_is_input(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return ios_state[io] & IO_AS_INPUT;
}


bool io_is_w1_now(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return ios_state[io] & IO_ONEWIRE;
}


bool io_is_pulsecount_now(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return ios_state[io] & IO_PULSE;
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

    if (ios_state[io] & IO_AS_INPUT)
        return;

    io_debug("%02u = %s", io, (on_off)?"ON":"OFF");

    if (on_off)
    {
        ios_state[io] |= IO_OUT_ON;
        gpio_set(output->port, output->pins);
    }
    else
    {
        ios_state[io] &= ~IO_OUT_ON;
        gpio_clear(output->port, output->pins);
    }
}


void     io_log(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * gpio_pin = &ios_pins[io];
    uint16_t io_state = ios_state[io];

    char * type = _ios_get_type(io_state);

    if (!(io_state & IO_ONEWIRE || io_state & IO_PULSE))
    {
        char * pretype = "";
        char * posttype = "";

        if (type[0])
        {
            if (io_is_special(io_state))
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
                    io_get_pull_str(io_state),
                    (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
        else
            log_out("IO %02u : %s%s%sOUT %s = %s",
                    io, pretype, type, posttype,
                    io_get_pull_str(io_state),
                    (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
    }
    else log_out("IO %02u : USED %s", io, type);
}


void     ios_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
        io_log(n);
}
