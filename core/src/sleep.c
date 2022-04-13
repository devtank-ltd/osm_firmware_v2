#include <stdio.h>

#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/lptimer.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/sync.h>


#include "sleep.h"
#include "log.h"
#include "common.h"
#include "uart_rings.h"
#include "measurements.h"


#define SLEEP_LSI_CLK_FREQ_KHZ          32


static volatile uint16_t _sleep_compare = 0;


static void _sleep_enter_sleep_mode(void)
{
    /* Set SLEEPONEXIT to ensure after interrupts will return to bed -- sorry, sleep. */
    SCB_SCR |= SCB_SCR_SLEEPONEXIT;

    /* Clear the SLEEPDEEP bit to ensure not a deep sleep */
    SCB_SCR &= ~SCB_SCR_SLEEPDEEP;

    /* Enter low power mode through WFI [Wake For Interrupt] */
    asm("wfi");
}


void sleep_exit_sleep_mode(void)
{
    if (!(SCB_SCR & SCB_SCR_SLEEPONEXIT))
    {
        sleep_debug("Not sleeping.");
        return;
    }
    /* Remove SLEEPONEXIT to bring the device out of sleep mode. */
    SCB_SCR &= ~SCB_SCR_SLEEPONEXIT;

    /* Wait for process to be completed before continuing */
    asm("dsb");
    lptimer_disable(LPTIM1);
}


static void _sleep_setup_tim(uint16_t count, uint32_t prescaler)
{
    nvic_disable_irq(NVIC_LPTIM1_IRQ);
    lptimer_disable(LPTIM1);

    /* Set up the clock for the low power timer LSI @ 32kHz */
    RCC_CCIPR = (RCC_CCIPR_LPTIMxSEL_LSI & RCC_CCIPR_LPTIMxSEL_MASK) << RCC_CCIPR_LPTIM1SEL_SHIFT;

    rcc_periph_clock_enable(RCC_LPTIM1);

    lptimer_set_internal_clock_source(LPTIM1);
    lptimer_enable_trigger(LPTIM1, LPTIM_CFGR_TRIGEN_SW);
    lptimer_set_prescaler(LPTIM1, prescaler);

    lptimer_enable(LPTIM1);

    lptimer_enable_preload(LPTIM1);
    lptimer_set_period(LPTIM1, 0xFFFF);
    _sleep_compare = count;
    lptimer_set_compare(LPTIM1, count);

    lptimer_enable_irq(LPTIM1, LPTIM_IER_CMPMIE);
    nvic_enable_irq(NVIC_LPTIM1_IRQ);

    lptimer_start_counter(LPTIM1, LPTIM_CR_SNGSTRT);
}


bool sleep_for_ms(uint32_t ms)
{
    uint32_t    before_time = get_since_boot_ms();
    sleep_debug("Sleeping for %"PRIu32"ms.", ms);
    while (uart_rings_out_busy())
    {
        uart_rings_out_drain();
        uart_ring_in_drain(LW_UART);
        uart_ring_in_drain(HPM_UART);
        uart_ring_in_drain(RS485_UART);
    }
    uint32_t    time_passed = since_boot_delta(get_since_boot_ms(), before_time);
    if (ms <= time_passed)
        return false;
    ms -= time_passed;
    if (ms < SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    /* Must sort out count and prescale depending on size of count */
    uint32_t    div_shift   = 0;
    uint64_t    count       = ms;
    uint16_t    count16;
    while (count >= (SLEEP_LSI_CLK_FREQ_KHZ * UINT16_MAX / 1000))
    {
        count /= 2;
        div_shift += 1;
        if (div_shift > 7)
        {
            count16 = 0xFFFF;
            div_shift = 7;
            goto turn_on_sleep;
        }
    }
    count = (ms * 1000) / (SLEEP_LSI_CLK_FREQ_KHZ * (1 << div_shift));
    count16 = count;
turn_on_sleep:
    _sleep_setup_tim(count16, (div_shift << LPTIM_CFGR_PRESC_SHIFT));
    _sleep_enter_sleep_mode();
    sleep_debug("Woken back up after %"PRIu32"ms.", since_boot_delta(get_since_boot_ms(), before_time));
    return true;
}


void lptim1_isr(void)
{
    if (lptimer_get_flag(LPTIM1, LPTIM_ICR_CMPMCF))
    {
        lptimer_clear_flag(LPTIM1, LPTIM_ICR_CMPMCF);
        sleep_exit_sleep_mode();
    }
}
