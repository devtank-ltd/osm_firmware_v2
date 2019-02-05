#include <inttypes.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"
#include "timers.h"
#include "adcs.h"

typedef struct
{
    uint32_t timer;
    uint32_t exti;
    unsigned adc;
    uint32_t adc_timer;
} pulsecount_exti_t;


typedef struct
{
    unsigned value;
    unsigned psp;
    unsigned tick;
    bool     is_high;
    unsigned high_timer_count;
    unsigned low_timer_count;
    unsigned high;
    unsigned low;
} pulsecount_values_t;

typedef struct
{
    uint32_t timer_clk;
    uint32_t irq;
    uint32_t adc_timer_clk;
    uint32_t adc_timer_irq;
} pps_init_t;


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

    if (exti->adc_timer != 0xFFFFFFFF)
    {
        values->tick = adcs_get_tick(exti->adc);
        timer_set_counter(exti->adc_timer, 0);
        timer_enable_counter(exti->adc_timer);
    }
}


static void pulsecount_adc_timer_isr(unsigned pps)
{
    const pulsecount_exti_t      * exti   = &pulsecount_extis[pps];
    volatile pulsecount_values_t * values = &pulsecount_values[pps];

    timer_clear_flag(exti->adc_timer, TIM_SR_CC1IF);

    if (values->tick != adcs_get_tick(exti->adc))
    {
        unsigned value = adcs_get_last(exti->adc);

        //Inversed now
        if (!values->is_high)
            values->high = value;
        else
            values->low = value;
        timer_disable_counter(exti->adc_timer);
    }
}


void PPS0_EXTI_ISR() { pulsecount_isr(0); }
void PPS1_EXTI_ISR() { pulsecount_isr(1); }

void PPS0_ADC_TIMER_ISR() { pulsecount_adc_timer_isr(0); }
void PPS1_ADC_TIMER_ISR() { pulsecount_adc_timer_isr(1); }


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
    const pps_init_t ppss_init[] = PPS_INIT;

    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
    {
        const pps_init_t        * pps_init = &ppss_init[n];
        const pulsecount_exti_t * exti = &pulsecount_extis[n];

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

        if (pps_init->adc_timer_clk != 0xFFFFFFFF)
        {
            rcc_periph_clock_enable(pps_init->adc_timer_clk);

            timer_disable_counter(exti->adc_timer);

            timer_set_mode(exti->adc_timer,
                           TIM_CR1_CKD_CK_INT,
                           TIM_CR1_CMS_EDGE,
                           TIM_CR1_DIR_UP);
            //-1 because it starts at zero, and interrupts on the overflow
            timer_set_prescaler(exti->adc_timer, rcc_ahb_frequency / 1000000-1);
            timer_set_period(exti->adc_timer, 100);

            timer_enable_preload(exti->adc_timer);
            timer_continuous_mode(exti->adc_timer);

            timer_set_counter(exti->adc_timer, 0);

            timer_enable_irq(exti->adc_timer, TIM_DIER_CC1IE);
            nvic_set_priority(pps_init->adc_timer_irq, 0);
            nvic_enable_irq(pps_init->adc_timer_irq);
        }

        /* -------------------------------------------------------- */


        const port_n_pins_t     * pins = &pps_pins[n];

        rcc_periph_clock_enable(PORT_TO_RCC(pins->port));
        gpio_mode_setup(pins->port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, pins->pins);

        rcc_periph_clock_enable(RCC_SYSCFG_COMP);

        exti_select_source(exti->exti, pins->port);
        exti_set_trigger(exti->exti, EXTI_TRIGGER_RISING);

        nvic_set_priority(pps_init->irq, 0);
        nvic_enable_irq(pps_init->irq);
    }

    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
    {
        volatile pulsecount_values_t * values = &pulsecount_values[n];
        const pulsecount_exti_t      * exti   = &pulsecount_extis[n];

        values->value   = 0;
        values->psp   = 0;
        values->is_high = false;

        timer_enable_counter(exti->timer);
        timer_set_counter(exti->timer, 0);
        exti_set_trigger(exti->exti, EXTI_TRIGGER_RISING);
        exti_enable_request(exti->exti);
    }
}


void pulsecount_pps_log(unsigned pps)
{
    if (pps < ARRAY_SIZE(pps_pins))
    {
        volatile pulsecount_values_t * values = &pulsecount_values[pps];
        unsigned duty =  (values->high_timer_count * 1000) /
                (values->high_timer_count + values->low_timer_count);
        log_out("pulsecount %u : %u : %u %u %u", pps, values->psp, duty, values->low, values->high);
    }
}


void pulsecount_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(pps_pins); n++)
        pulsecount_pps_log(n);
}
