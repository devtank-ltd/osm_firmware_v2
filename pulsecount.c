#include <inttypes.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"

typedef struct
{
    uint32_t exti;
    uint32_t exti_irq;
} pulsecount_exti_t;


typedef struct
{
    unsigned value;
    unsigned psp;
} pulsecount_values_t;

static const port_n_pins_t     pps_pins[] = PPS_PORT_N_PINS;
static const pulsecount_exti_t pulsecount_extis[] = PPS_EXTI;

static volatile pulsecount_values_t pulsecount_values[ARRAY_SIZE(pps_pins)] = {{0}};


static void pulsecount_isr(unsigned ppss)
{
    exti_reset_request(pulsecount_extis[ppss].exti);
    pulsecount_values[ppss].value++;
}


void PPS0_EXTI_ISR() { pulsecount_isr(0); }
void PPS1_EXTI_ISR() { pulsecount_isr(1); }


void pulsecount_second_boardary()
{
    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
    {
        pulsecount_values[n].psp   = pulsecount_values[n].value;
        pulsecount_values[n].value = 0;
    }
}


unsigned pulsecount_get_count()
{
    return ARRAY_SIZE(pps_pins);
}


void pulsecount_get(unsigned pps, unsigned * count)
{
    if (pps >= ARRAY_SIZE(pps_pins))
        *count = 0;
    else
        *count = pulsecount_values[pps].psp;
}


void pulsecount_init()
{
    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(pps_pins[n].port));
        gpio_mode_setup(pps_pins[n].port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, pps_pins[n].pins);

        rcc_periph_clock_enable(RCC_SYSCFG_COMP);

        exti_select_source(pulsecount_extis[n].exti, pps_pins[n].port);
        exti_set_trigger(pulsecount_extis[n].exti, EXTI_TRIGGER_FALLING);
        exti_enable_request(pulsecount_extis[n].exti);

        nvic_set_priority(pulsecount_extis[n].exti_irq, 2);
        nvic_enable_irq(pulsecount_extis[n].exti_irq);
    }
}


void pulsecount_pps_log(unsigned pps)
{
    if (pps < ARRAY_SIZE(pps_pins))
        log_out("pulsecount %u : %u", pps, pulsecount_values[pps].psp);

}


void pulsecount_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
        pulsecount_pps_log(n);
}
