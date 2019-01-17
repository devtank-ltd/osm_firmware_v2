#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "inputs.h"
#include "log.h"

typedef struct
{
    unsigned on_count;
    unsigned off_count;
} __attribute__((packed)) input_count_t;


static const port_n_pins_t    inputs[] = INPUTS_PORT_N_PINS;
static input_count_t          inputs_count[ARRAY_SIZE(inputs)]     = {0};
static volatile input_count_t inputs_cur_count[ARRAY_SIZE(inputs)] = {0};


void     inputs_init()
{

    for(unsigned n = 0; n < ARRAY_SIZE(inputs); n++)
    {
        const port_n_pins_t * input = &inputs[n];
        rcc_periph_clock_enable(PORT_TO_RCC(input->port));
        gpio_mode_setup(input->port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, input->pins);
    }
}


void     inputs_do_sample()
{
    for(unsigned n = 0; n < ARRAY_SIZE(inputs); n++)
    {
        const port_n_pins_t * input = &inputs[n];
        if (gpio_get(input->port, input->pins))
            inputs_count[n].on_count++;
        else
            inputs_count[n].off_count++;
    }
}


void     inputs_second_boardary()
{
    memcpy((input_count_t*)inputs_cur_count, inputs_count, sizeof(inputs_count));
    memset(inputs_count, 0, sizeof(inputs_count));
}


unsigned inputs_get_count()
{
    return ARRAY_SIZE(inputs);
}


void     inputs_get_input_counts(unsigned input, unsigned * on_count, unsigned * off_count)
{
    if (input >= ARRAY_SIZE(inputs))
    {
        *on_count  = 0;
        *off_count = 0;
        return;
    }

    volatile input_count_t * input_cur_count = &inputs_cur_count[input];

    *on_count  = input_cur_count->on_count;
    *off_count = input_cur_count->off_count;
}


void     inputs_log()
{
    log_out(LOG_SPACER);
    for(unsigned n = 0; n < ARRAY_SIZE(inputs); n++)
    {
        volatile input_count_t * input_cur_count = &inputs_cur_count[n];
        log_out("Input %02u : %04u %04u", n,
                                          input_cur_count->on_count,
                                          input_cur_count->off_count);
    }
    log_out(LOG_SPACER);
}
