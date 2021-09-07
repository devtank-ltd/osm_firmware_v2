#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "io.h"
#include "log.h"
#include "pulsecount.h"
#include "uarts.h"

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
        case IO_PPS0: return "PPS0";
        case IO_PPS1: return "PPS1";
        case IO_RELAY : return "RL";
        case IO_HIGHSIDE : return "HS";
        case IO_UART0 :
            if (io_state & IO_UART_TX)
                return "UART0_TX";
            else
                return "UART0_RX";
        case IO_UART1 :
            if (io_state & IO_UART_TX)
                return "UART1_TX";
            else
                return "UART1_RX";
        default : return "";
    }
}


static bool _io_is_special(uint16_t io_state)
{
    switch(io_state & IO_TYPE_MASK)
    {
        case IO_PPS0:
        case IO_PPS1:
        case IO_UART0:
        case IO_UART1:
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

    ios_state[io] = (ios_state[io] & (IO_TYPE_MASK | IO_UART_TX)) | io_state;

    log_debug(DEBUG_IO, "IO %02u set to %s %s",
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
            log_debug(DEBUG_IO, "IO %02u : USED %s", n, _ios_get_type(io_state));
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

        if (io_type == IO_PPS0)
        {
           // pulsecount_enable(0, false);
            io_state &= ~IO_SPECIAL_EN;
            log_debug(DEBUG_IO, "IO %02u : PPS0 NO LONGER", io);
        }
        else if (io_type == IO_PPS1)
        {
            //pulsecount_enable(1, false);
            io_state &= ~IO_SPECIAL_EN;
            log_debug(DEBUG_IO, "IO %02u : PPS1 NO LONGER", io);
        }
        else if (io_type == IO_UART0)
        {
            /* Public uart 0, but is really uart 1 */
            uart_enable(1, false);

            unsigned io_twin = (io_state & IO_UART_TX)?io-1:io+1;

            io_state &= ~IO_SPECIAL_EN;

            if (ios_state[io_twin] & IO_SPECIAL_EN)
            {
                ios_state[io_twin] &= ~IO_SPECIAL_EN;
                io_configure(io_twin, true, 0);
            }

            log_debug(DEBUG_IO, "IO %02u : UART0 NO LONGER", io);
            log_debug(DEBUG_IO, "IO %02u : UART0 NO LONGER", io_twin);
        }
        else if (io_type == IO_UART1)
        {
            /* Public uart 1, but is really uart 2 */
            uart_enable(2, false);

            unsigned io_twin = (io_state & IO_UART_TX)?io-1:io+1;

            io_state &= ~IO_SPECIAL_EN;

            if (ios_state[io_twin] & IO_SPECIAL_EN)
            {
                ios_state[io_twin] &= ~IO_SPECIAL_EN;
                io_configure(io_twin, true, 0);
            }

            log_debug(DEBUG_IO, "IO %02u : UART1 NO LONGER", io);
            log_debug(DEBUG_IO, "IO %02u : UART1 NO LONGER", io_twin);
        }
        else
        {
            log_debug(DEBUG_IO, "IO %02u : USED %s", io, _ios_get_type(io_state));
            return;
        }
    }

    if (io_state & IO_DIR_LOCKED)
    {
        log_debug(DEBUG_IO, "GPIO %02u locked but change attempted.", io);
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

    if (io_type == IO_PPS0)
    {
        //pulsecount_enable(0, true);
        ios_state[io] |= IO_SPECIAL_EN;
        log_debug(DEBUG_IO, "IO %02u : USED PPS0", io);
        return true;
    }
    else if (io_type == IO_PPS1)
    {
        //pulsecount_enable(1, true);
        ios_state[io] |= IO_SPECIAL_EN;
        log_debug(DEBUG_IO, "IO %02u : USED PPS0", io);
        return true;
    }
    else if (io_type == IO_UART0)
    {
        unsigned io_twin = (io_state & IO_UART_TX)?io-1:io+1;
        /* Public uart 0, but is really uart 1 */
        uart_enable(1, true);
        ios_state[io] |= IO_SPECIAL_EN;
        ios_state[io_twin] |= IO_SPECIAL_EN;
        log_debug(DEBUG_IO, "IO %02u : USED UART0", io);
        log_debug(DEBUG_IO, "IO %02u : USED UART0", io_twin);
        return true;
    }
    else if (io_type == IO_UART1)
    {
        unsigned io_twin = (io_state & IO_UART_TX)?io-1:io+1;
        /* Public uart 1, but is really uart 2 */
        uart_enable(2, true);
        ios_state[io] |= IO_SPECIAL_EN;
        ios_state[io_twin] |= IO_SPECIAL_EN;
        log_debug(DEBUG_IO, "IO %02u : USED UART1", io);
        log_debug(DEBUG_IO, "IO %02u : USED UART1", io_twin);
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

    log_debug(DEBUG_IO, "IO %02u = %s", io, (on_off)?"ON":"OFF");

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
