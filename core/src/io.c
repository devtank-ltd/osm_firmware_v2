#include <string.h>

#include "config.h"
#include "pinmap.h"
#include "io.h"
#include "log.h"
#include "pulsecount.h"
#include "uarts.h"
#include "platform.h"
#include "ds18b20.h"
#include "persist_config.h"

static const port_n_pins_t ios_pins[]           = IOS_PORT_N_PINS;
static uint16_t * ios_state;


static char* _ios_get_type_active(uint16_t io_state)
{
    switch(io_state & IO_TYPE_ON_MASK)
    {
        case IO_PULSE: return "PLSCNT";
        case IO_ONEWIRE:    return "W1";
        default : return "";
    }
}


static char* _ios_get_type_possible(uint16_t io_state)
{
    switch(io_state & IO_TYPE_MASK)
    {
        case IO_TYPE_PULSECOUNT: return "PLSCNT";
        case IO_TYPE_ONEWIRE:    return "W1";
        case IO_TYPE_ONEWIRE | IO_TYPE_PULSECOUNT: return "PLSCNT | W1";
        default : return "";
    }
}


bool ios_get_pupd(unsigned io, uint8_t* pupd)
{
    if (!pupd)
    {
        io_debug("Handed NULL pointer.");
        return false;
    }
    if (io >= IOS_COUNT)
    {
        io_debug("IO '%u' out of range.", io);
        return false;
    }
    *pupd = ios_state[io] & IO_PULL_MASK;
    return true;
}


static void _ios_setup_gpio(unsigned io, uint16_t io_state)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    char * type = _ios_get_type_possible(io_state);

    const port_n_pins_t * gpio_pin = &ios_pins[io];

    platform_gpio_setup(gpio_pin, io_state & IO_AS_INPUT, io_state & IO_PULL_MASK);

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
        platform_gpio_init(&ios_pins[n]);

        uint16_t io_state = ios_state[n];

        if (io_state & IO_TYPE_ON_MASK)
            io_debug("%02u : USED %s", n, _ios_get_type_active(io_state));
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
    ios_state[io] &= ~IO_AS_INPUT;

    ios_state[io] |= IO_ONEWIRE;
    ds18b20_enable(true);
    io_debug("%02u : USED W1", io);
    return true;
}


static unsigned io_pull(io_pupd_t pull)
{
    switch(pull)
    {
        case IO_PUPD_NONE:
            return GPIO_PUPD_NONE;
        case IO_PUPD_UP:
            return GPIO_PUPD_PULLUP;
        case IO_PUPD_DOWN:
            return GPIO_PUPD_PULLDOWN;
    }
    return GPIO_PUPD_NONE;
}


bool io_enable_pulsecount(unsigned io, io_pupd_t pupd)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    if (!(ios_state[io] & IO_TYPE_PULSECOUNT))
        return false;

    ios_state[io] &= ~IO_ONEWIRE;
    ios_state[io] &= ~IO_OUT_ON;
    ios_state[io] &= ~IO_AS_INPUT;

    ios_state[io] &= ~IO_PULL_MASK;
    ios_state[io] |= (io_pull(pupd) & IO_PULL_MASK);

    ios_state[io] |= IO_PULSE;
    pulsecount_enable(true);
    io_debug("%02u : USED PLSCNT", io);
    return true;
}


void     io_configure(unsigned io, bool as_input, io_pupd_t pull)
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
        ds18b20_enable(false);
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
    io_state |= (io_pull(pull) & IO_PULL_MASK);

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
        platform_gpio_set(output, true);
    }
    else
    {
        ios_state[io] &= ~IO_OUT_ON;
        platform_gpio_set(output, false);
    }
}


void     io_log(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * gpio_pin = &ios_pins[io];
    uint16_t io_state = ios_state[io];

    if (!(io_state & IO_TYPE_ON_MASK))
    {
        char * pretype = "";
        char * posttype = "";

        char * type = _ios_get_type_possible(io_state);

        if (type[0])
        {
            pretype = "[";
            posttype = "] ";
        }
        else
        {
            pretype = "";
            posttype = " ";
        }

        if (io_state & IO_AS_INPUT)
            log_out("IO %02u : %s%s%sIN %s = %s",
                    io, pretype, type, posttype,
                    io_get_pull_str(io_state),
                    (platform_gpio_get(gpio_pin))?"ON":"OFF");
        else
            log_out("IO %02u : %s%s%sOUT %s = %s",
                    io, pretype, type, posttype,
                    io_get_pull_str(io_state),
                    (platform_gpio_get(gpio_pin))?"ON":"OFF");
    }
    else
    {
        char * type = _ios_get_type_active(io_state);
        char pupd_char;
        if ((io_state & IO_PULL_MASK) == GPIO_PUPD_PULLUP)
            pupd_char = 'U';
        else if ((io_state & IO_PULL_MASK) == GPIO_PUPD_PULLDOWN)
            pupd_char = 'D';
        else if ((io_state & IO_PULL_MASK) == GPIO_PUPD_NONE)
            pupd_char = 'N';
        else
            pupd_char = ' ';
        log_out("IO %02u : USED %s %c", io, type, pupd_char);
    }
}


void     ios_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
        io_log(n);
}
