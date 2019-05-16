#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "inputs.h"
#include "log.h"

static const port_n_pins_t inputs[] = INPUTS_PORT_N_PINS;


void     inputs_init()
{
    const uint8_t pulls[ARRAY_SIZE(inputs)] = INPUT_PULL;

    for(unsigned n = 0; n < ARRAY_SIZE(inputs); n++)
    {
        const port_n_pins_t * input = &inputs[n];
        rcc_periph_clock_enable(PORT_TO_RCC(input->port));
        gpio_mode_setup(input->port, GPIO_MODE_INPUT, pulls[n], input->pins);
    }
}


unsigned inputs_get_count()
{
    return ARRAY_SIZE(inputs);
}


void     inputs_input_log(unsigned input)
{
    if (input >= ARRAY_SIZE(inputs))
        return;

    const port_n_pins_t * input_gpio = &inputs[input];
    log_out("Input %02u : %s", input,
        (gpio_get(input_gpio->port, input_gpio->pins))?"ON":"OFF");
}


void     inputs_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(inputs); n++)
        inputs_input_log(n);
}
