#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/lptimer.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/sync.h>


#include <osm/core/sleep.h>
#include <osm/core/log.h>
#include <osm/core/common.h>
#include <osm/core/uart_rings.h>
#include <osm/core/measurements.h>
#include "pinmap.h"
#include <osm/core/adcs.h>
#include <osm/core/i2c.h>


#define SLEEP_LSI_CLK_FREQ_KHZ          32


static volatile uint16_t _sleep_compare = 0;


void _sleep_before_sleep(void)
{
    adcs_off();
    i2cs_deinit();
    iwdg_set_period_ms(IWDG_MAX_TIME_MS);
}


void _sleep_on_wakeup(void)
{
    adcs_init();
    i2cs_init();
    iwdg_set_period_ms(IWDG_NORMAL_TIME_MS);
}


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
    if (ms > SLEEP_MAX_TIME_MS)
        ms = SLEEP_MAX_TIME_MS;
    else if (ms < SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    uint32_t    before_time = get_since_boot_ms();
    sleep_debug("Sleeping for %"PRIu32"ms.", ms);
    while (uart_rings_out_busy())
    {
        uart_rings_out_drain();
        uart_ring_in_drain(COMMS_UART);
    }
    uint32_t    time_passed = since_boot_delta(get_since_boot_ms(), before_time);
    if (ms <= time_passed)
        return false;
    ms -= time_passed;
    if (ms < SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    ms -= SLEEP_MIN_SLEEP_TIME_MS;
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
            // Reduced the maximum value of sleep by 1 count so theres enough time for watchdog.
            count16 = 0xFFFE;
            div_shift = 7;
            goto turn_on_sleep;
        }
    }
    count = (ms * 1000) / (SLEEP_LSI_CLK_FREQ_KHZ * (1 << div_shift));
    count16 = count;
turn_on_sleep:
    _sleep_before_sleep();
    _sleep_setup_tim(count16, (div_shift << LPTIM_CFGR_PRESC_SHIFT));
    _sleep_enter_sleep_mode();
    _sleep_on_wakeup();
    sleep_debug("Woken back up after %"PRIu32"ms.", since_boot_delta(get_since_boot_ms(), before_time));
    return true;
}


// cppcheck-suppress unusedFunction ; System handler
void lptim1_isr(void)
{
    if (lptimer_get_flag(LPTIM1, LPTIM_ICR_CMPMCF))
    {
        lptimer_clear_flag(LPTIM1, LPTIM_ICR_CMPMCF);
        sleep_exit_sleep_mode();
    }
}


static command_response_t _sleep_cb(char* args, cmd_ctx_t * ctx)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        cmd_ctx_out(ctx, "<TIME(MS)>");
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx, "Sleeping for %"PRIu32"ms.", sleep_ms);
    sleep_for_ms(sleep_ms);
    return COMMAND_RESP_OK;
}


static command_response_t _sleep_power_mode_cb(char* args, cmd_ctx_t * ctx)
{
    measurements_power_mode_t mode;
    if (args[0] == 'A')
        mode = MEASUREMENTS_POWER_MODE_AUTO;
    else if (args[0] == 'B')
        mode = MEASUREMENTS_POWER_MODE_BATTERY;
    else if (args[0] == 'P')
        mode = MEASUREMENTS_POWER_MODE_PLUGGED;
    else
        return COMMAND_RESP_ERR;
    measurements_power_mode(mode);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* sleep_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "sleep",        "Sleep",                    _sleep_cb                      , false , NULL },
                                       { "power_mode",   "Power mode setting",       _sleep_power_mode_cb           , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
