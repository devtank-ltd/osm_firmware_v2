#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "gpio.h"
#include "log.h"

static const port_n_pins_t gpios_pins[] = GPIOS_PORT_N_PINS;
static uint16_t gpios_state[ARRAY_SIZE(gpios_pins)] = GPIOS_STATE;


#define GPIO_PULL_STR_NONE "NONE"
#define GPIO_PULL_STR_UP   "UP"
#define GPIO_PULL_STR_DOWN "DOWN"


static char* _gpios_get_pull_str(uint16_t gpio_state)
{
    switch(gpio_state & GPIO_PULL_MASK)
    {
        case GPIO_PUPD_PULLUP:   return GPIO_PULL_STR_UP;
        case GPIO_PUPD_PULLDOWN: return GPIO_PULL_STR_DOWN;
        default:
            break;
    }
    return GPIO_PULL_STR_NONE;
}


static char* _gpios_get_type(uint16_t gpio_state)
{
    uint16_t gpio_type  = gpio_state & GPIO_TYPE_MASK;

    switch(gpio_type)
    {
        case GPIO_PPS1: return "PPS1";
        case GPIO_PPS2: return "PPS2";
        case GPIO_RELAY : return "RL";
        case GPIO_HIGHSIDE : return "HS";
        case GPIO_UART1 :
            if (gpio_state & GPIO_UART_TX)
                return "UART1_TX";
            else
                return "UART1_RX";
        case GPIO_UART2 :
            if (gpio_state & GPIO_UART_TX)
                return "UART2_TX";
            else
                return "UART2_RX";
        default : return "";
    }
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

    log_debug(DEBUG_GPIO, "IO %02u set to %s %s",
            gpio,
            (gpio_state & GPIO_AS_INPUT)?"IN":"OUT",
            _gpios_get_pull_str(gpio_state));
}


void     gpios_init()
{
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(gpios_pins[n].port));

        uint16_t gpio_state = gpios_state[n];

        if (gpio_state & GPIO_SPECIAL_EN)
            log_debug(DEBUG_GPIO, "IO %02u : USED %s", n, _gpios_get_type(gpio_state));
        else
            _gpios_setup_gpio(n, gpio_state);
    }
}


unsigned gpios_get_count()
{
    return ARRAY_SIZE(gpios_pins);
}


void     gpio_configure(unsigned gpio, bool as_input, int pull)
{
    if (gpio >= ARRAY_SIZE(gpios_pins))
        return;

    uint16_t gpio_state = gpios_state[gpio];

    if (gpio_state & GPIO_SPECIAL_EN)
    {
        log_debug(DEBUG_GPIO, "IO %02u : USED %s", gpio, _gpios_get_type(gpio_state));
        return;
    }

    gpio_state = (as_input)?GPIO_AS_INPUT:0;

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

    log_debug(DEBUG_GPIO, "IO %02u = %s", gpio, (on_off)?"ON":"OFF");

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

    char * type = _gpios_get_type(gpio_state);

    if (!(gpio_state & GPIO_SPECIAL_EN))
    {
        char * pretype = "";
        char * posttype = "";

        if (type[0])
        {
            pretype = "[";
            posttype = "]";
        }

        if (gpio_state & GPIO_AS_INPUT)
            log_out("IO %02u : %s%s%sIN %s = %s",
                    gpio, pretype, type, posttype,
                    _gpios_get_pull_str(gpio_state),
                    (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
        else
            log_out("IO %02u : %s%s%sOUT = %s",
                    gpio, pretype, type, posttype,
                    (gpio_get(gpio_pin->port, gpio_pin->pins))?"ON":"OFF");
    }
    else log_out("IO %02u : USED %s", gpio, type);
}


void     gpios_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(gpios_pins); n++)
        gpio_log(n);
}
