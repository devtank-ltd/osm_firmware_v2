#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "gpio.h"
#include "log.h"

static const port_n_pins_t gpios_pins[] = GPIOS_PORT_N_PINS;
static uint16_t gpios_state[ARRAY_SIZE(gpios_pins)] = {0};


static char* _gpios_get_pull_str(uint16_t gpio_state)
{
    switch(gpio_state & GPIO_PULL_MASK)
    {
        case GPIO_PUPD_PULLUP:   return "UP";
        case GPIO_PUPD_PULLDOWN: return "DOWN";
        default:
            break;
    }
    return "NONE";
}

static void _gpios_setup_gpio(unsigned gpio, uint16_t gpio_state)
{
    if (gpio >= ARRAY_SIZE(gpios_pins))
        return;

    if (gpios_state[gpio] & GPIO_DIR_LOCKED)
    {
        log_debug(DEBUG_GPIO, "GPIO %02u locked but change attempted.", gpio);
        return;
    }

    const port_n_pins_t * gpio_pin = &gpios_pins[gpio];

    gpio_mode_setup(gpio_pin->port,
        (gpio_state & GPIO_AS_INPUT)?GPIO_MODE_INPUT:GPIO_MODE_OUTPUT,
        gpio_state & GPIO_PULL_MASK,
        gpio_pin->pins);

    gpios_state[gpio] = gpio_state;

    log_debug(DEBUG_GPIO, "GPIO %02u set to %s %s",
            gpio,
            (gpio_state & GPIO_AS_INPUT)?"IN":"OUT",
            _gpios_get_pull_str(gpio_state));
}


void     gpios_init()
{
    const uint16_t gpios_init_state[] = GPIOS_STATE;

    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(gpios_pins[n].port));

        _gpios_setup_gpio(n, gpios_init_state[n]);
    }
}


unsigned gpios_get_count()
{
    return ARRAY_SIZE(gpios_pins);
}


unsigned inputs_get_count()
{
    unsigned r = 0;
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        if (gpios_state[n] & GPIO_AS_INPUT)
            r++;
    return r;
}


unsigned outputs_get_count()
{
    unsigned r = 0;
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        if (!(gpios_state[n] & GPIO_AS_INPUT))
            r++;
    return r;
}


static int gpio_get_input(unsigned input)
{
    unsigned r = 0;
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        if (gpios_state[n] & GPIO_AS_INPUT)
        {
            if (r == input)
            {
                log_debug(DEBUG_GPIO, "Input %u = gpio %u", input, n);
                return n;
            }
            else r++;
        }
    return -1;
}

static int gpio_get_output(unsigned output)
{
    unsigned r = 0;
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        if (!(gpios_state[n] & GPIO_AS_INPUT))
        {
            if (r == output)
            {
                log_debug(DEBUG_GPIO, "Output %u = gpio %u", output, n);
                return n;
            }
            else r++;
        }
    return -1;
}


void     _inputs_input_log(unsigned input, int gpio)
{
    if (gpio < 0)
        return;

    const port_n_pins_t * input_gpio = &gpios_pins[gpio];
    log_out("Input %02u : %s", input,
        (gpio_get(input_gpio->port, input_gpio->pins))?"ON":"OFF");
}

void     inputs_input_log(unsigned input)
{
    _inputs_input_log(input, gpio_get_input(input));
}

void     inputs_log()
{
    unsigned r = 0;
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        if (gpios_state[n] & GPIO_AS_INPUT)
        {
            _inputs_input_log(r, n);
            r++;
        }
}


void     _output_output_log(unsigned output, int gpio)
{
    if (gpio < 0)
        return;

    const port_n_pins_t * output_gpio = &gpios_pins[gpio];
    uint16_t on_off = gpio_get(output_gpio->port, output_gpio->pins);
    log_out("Output %02u : %s", output, (on_off)?"ON":"OFF");
}


void     output_output_log(unsigned output)
{
    _output_output_log(output, gpio_get_output(output));
}


void     outputs_log()
{
    unsigned r = 0;
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        if (!(gpios_state[n] & GPIO_AS_INPUT))
        {
            _output_output_log(r, n);
            r++;
        }
}


void     outputs_set(unsigned index, bool on_off)
{
    int gpio = gpio_get_output(index);
    if (gpio < 0)
        return;

    gpio_on(gpio, on_off);
}


void     gpio_configure(unsigned gpio, bool as_input, int pull)
{
    if (gpio >= ARRAY_SIZE(gpios_pins))
        return;

    uint16_t gpio_state = (as_input)?GPIO_AS_INPUT:0;

    if (pull > 0)
        gpio_state |= GPIO_PUPD_PULLUP;
    else if (pull < 0)
        gpio_state |= GPIO_PUPD_PULLDOWN;

    _gpios_setup_gpio(gpio, gpio_state);
}


bool     gpio_is_input(unsigned gpio)
{
    if (gpio >= ARRAY_SIZE(gpios_pins))
        return false;

    return (gpios_state[gpio] & GPIO_AS_INPUT)?true:false;
}


void     gpio_on(unsigned gpio, bool on_off)
{
    if (gpio >= ARRAY_SIZE(gpios_pins))
        return;

    const port_n_pins_t * output = &gpios_pins[gpio];

    log_debug(DEBUG_GPIO, "GPIO %02u = %s", gpio, (on_off)?"ON":"OFF");

    if (on_off)
        gpio_set(output->port, output->pins);
    else
        gpio_clear(output->port, output->pins);
}


void     gpio_log(unsigned gpio)
{
    if (gpio >= ARRAY_SIZE(gpios_pins))
        return;

    const port_n_pins_t * gpio_pin = &gpios_pins[gpio];
    uint16_t gpio_state = gpios_state[gpio];

    log_out("GPIO %02u : %s %s = %s",
            gpio,
            (gpio_state & GPIO_AS_INPUT)?"IN":"OUT",
            _gpios_get_pull_str(gpio_state),
            (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
}


void     gpios_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        gpio_log(n);
}
