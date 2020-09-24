#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"
#include "timers.h"

typedef struct
{
    uint32_t timer;
    uint32_t exti;
} pulsecount_exti_t;


typedef struct
{
    unsigned value;
    unsigned psp;
    unsigned tick;
    bool     is_high;
    unsigned high_timer_count;
    unsigned low_timer_count;
} pulsecount_values_t;

typedef struct
{
    uint32_t timer_clk;
    uint32_t irq;
} pps_init_t;


static const pps_init_t        ppss_init[]        = PPS_INIT;
static const port_n_pins_t     pps_pins[]         = PPS_PORT_N_PINS;
static const pulsecount_exti_t pulsecount_extis[] = PPS_EXTI;

static volatile pulsecount_values_t pulsecount_values[ARRAY_SIZE(pps_pins)] = {{0}};


static void pulsecount_isr(unsigned pps)
{
    const pulsecount_exti_t      * exti   = &pulsecount_extis[pps];
    volatile pulsecount_values_t * values = &pulsecount_values[pps];

    exti_reset_request(exti->exti);

    if (values->is_high)
    {
        values->value++;
        exti_set_trigger(exti->exti, EXTI_TRIGGER_FALLING);
    }
    else exti_set_trigger(exti->exti, EXTI_TRIGGER_RISING);

    values->is_high = !values->is_high;

    unsigned timer_count = timer_get_counter(exti->timer);
    timer_set_counter(exti->timer, 0);

    if (values->is_high)
        values->high_timer_count = timer_count;
    else
        values->low_timer_count  = timer_count;
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


static void _pulsecount_init(unsigned pps)
{
    const pps_init_t        * pps_init = &ppss_init[pps];
    const pulsecount_exti_t * exti = &pulsecount_extis[pps];

    rcc_periph_clock_enable(pps_init->timer_clk);

    timer_disable_counter(exti->timer);

    timer_set_mode(exti->timer,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow
    timer_set_prescaler(exti->timer, rcc_ahb_frequency / 1000000-1);
    timer_set_period(exti->timer, 10000000-1);

    timer_enable_preload(exti->timer);
    timer_continuous_mode(exti->timer);

    timer_set_counter(exti->timer, 0);

    /* -------------------------------------------------------- */


    const port_n_pins_t     * pins = &pps_pins[pps];

    rcc_periph_clock_enable(PORT_TO_RCC(pins->port));
    gpio_mode_setup(pins->port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, pins->pins);

    rcc_periph_clock_enable(RCC_SYSCFG_COMP);

    exti_select_source(exti->exti, pins->port);
    exti_set_trigger(exti->exti, EXTI_TRIGGER_RISING);

    nvic_set_priority(pps_init->irq, PPS_PRIORITY);
    nvic_enable_irq(pps_init->irq);


    volatile pulsecount_values_t * values = &pulsecount_values[pps];

    values->value   = 0;
    values->psp   = 0;
    values->is_high = false;

    timer_enable_counter(exti->timer);
    timer_set_counter(exti->timer, 0);
    exti_set_trigger(exti->exti, EXTI_TRIGGER_RISING);
    exti_enable_request(exti->exti);
}


static void _pulsecount_shutdown(unsigned pps)
{
    const pps_init_t        * pps_init = &ppss_init[pps];
    const pulsecount_exti_t * exti = &pulsecount_extis[pps];

    rcc_periph_clock_enable(pps_init->timer_clk);

    timer_disable_counter(exti->timer);
    exti_disable_request(exti->exti);

    nvic_disable_irq(pps_init->irq);

    memset((pulsecount_values_t*)&pulsecount_values[pps], 0, sizeof(pulsecount_values_t));
}


void pulsecount_init()
{
    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
        _pulsecount_init(n);
}


void     pulsecount_enable(unsigned pps, bool enable)
{
    if (pps >= ARRAY_SIZE(pps_pins))
        return;

    if (enable)
        _pulsecount_init(pps);
    else
        _pulsecount_shutdown(pps);
}


void pulsecount_pps_log(unsigned pps)
{
    if (pps < ARRAY_SIZE(pps_pins))
    {
        volatile pulsecount_values_t * values = &pulsecount_values[pps];
        unsigned duty =  (values->high_timer_count * 1000) /
                (values->high_timer_count + values->low_timer_count);
        log_out("pulsecount %u : %u : %u", pps, values->psp, duty);
    }
}


void pulsecount_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
        pulsecount_pps_log(n);
}
