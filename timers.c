#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include "config.h"
#include "adcs.h"
#include "pulsecount.h"
#include "timers.h"
#include "uart_rings.h"
#include "log.h"

static volatile unsigned fast_rate_sps     = DEFAULT_SPS;
static volatile unsigned fast_sample_count = DEFAULT_SPS-1;

static volatile unsigned adc_timer_boardary = DEFAULT_SPS;


extern bool     timer_set_adc_boardary(unsigned ms)
{
    uint64_t t = DEFAULT_SPS;
    t *= ms;
    t /= 1000;

    uint64_t t2 = t * 1000ULL;

    unsigned check = t2 / DEFAULT_SPS;

    if (check != ms)
        return true;

    log_debug(DEBUG_SYS, "Unable to set ADC boardary to %u", ms);
    log_debug(DEBUG_SYS, "Closest was %u ms", check);
    return false;
}


extern unsigned timer_get_adc_boardary()
{
    uint64_t sps = adc_timer_boardary * 1000ULL;
    return sps / DEFAULT_SPS;
}


void tim3_isr(void)
{
    pulsecount_second_boardary();

    timer_clear_flag(TIM3, TIM_SR_CC1IF);
}


void tim2_isr(void)
{
    fast_sample_count++;

    if (fast_sample_count >= adc_timer_boardary)
    {
        adcs_collect_boardary();
        fast_sample_count = 0;
    }

    adcs_do_samples();
    timer_clear_flag(TIM2, TIM_SR_CC1IF);
}


void     timer_wait()
{
    unsigned cur = fast_sample_count;
    while(cur < fast_sample_count);
}


void     timers_init()
{
    rcc_periph_clock_enable(RCC_TIM2);
    rcc_periph_clock_enable(RCC_TIM3);

    timer_disable_counter(TIM3);

    timer_set_mode(TIM3,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow
    timer_set_prescaler(TIM3, rcc_ahb_frequency / 1000-1);
    timer_set_period(TIM3, 1000-1);

    timer_enable_preload(TIM3);
    timer_continuous_mode(TIM3);

    timer_enable_counter(TIM3);
    timer_enable_irq(TIM3, TIM_DIER_CC1IE);

    timer_set_counter(TIM3, 0);
    nvic_enable_irq(NVIC_TIM3_IRQ);
    nvic_set_priority(NVIC_TIM3_IRQ, TIMER1_PRIORITY);



    timer_disable_counter(TIM2);

    timer_set_mode(TIM2,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow
    timer_set_prescaler(TIM2, rcc_ahb_frequency / 1000000-1);
    timer_set_period(TIM2, 1000 * 1000 / DEFAULT_SPS - 1);
    timer_enable_preload(TIM2);
    timer_continuous_mode(TIM2);

    timer_enable_counter(TIM2);
    timer_enable_irq(TIM2, TIM_DIER_CC1IE);

    timer_set_counter(TIM2, 0);
    nvic_enable_irq(NVIC_TIM2_IRQ);
    nvic_set_priority(NVIC_TIM2_IRQ, TIMER2_PRIORITY);
}
