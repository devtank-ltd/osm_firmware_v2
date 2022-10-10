#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "sleep.h"

#include "log.h"
#include "common.h"
#include "uart_rings.h"
#include "measurements.h"
#include "adcs.h"
#include "platform.h"
#include "linux.h"


#define SLEEP_LSI_CLK_FREQ_KHZ          32


static volatile uint16_t _sleep_compare = 0;


void _sleep_before_sleep(void)
{
    adcs_off();
    platform_watchdog_init(IWDG_MAX_TIME_MS);
}


void _sleep_on_wakeup(void)
{
    adcs_init();
    platform_watchdog_init(IWDG_NORMAL_TIME_MS);
}


void sleep_exit_sleep_mode(void)
{
    ;
}


bool sleep_for_ms(uint32_t ms)
{
    if (ms > SLEEP_MAX_TIME_MS)
        ms = SLEEP_MAX_TIME_MS;
    uint32_t    before_time = get_since_boot_ms();
    sleep_debug("Sleeping for %"PRIu32"ms.", ms);
    while (uart_rings_out_busy())
    {
        uart_rings_out_drain();
        uart_ring_in_drain(LW_UART);
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
    while (count >= (SLEEP_LSI_CLK_FREQ_KHZ * UINT16_MAX / 1000))
    {
        count /= 2;
        div_shift += 1;
        if (div_shift > 7)
        {
            // Reduced the maximum value of sleep by 1 count so theres enough time for watchdog.
            div_shift = 7;
            goto turn_on_sleep;
        }
    }
    count = (ms * 1000) / (SLEEP_LSI_CLK_FREQ_KHZ * (1 << div_shift));
turn_on_sleep:
    _sleep_before_sleep();
    linux_usleep(ms * 1000);
    _sleep_on_wakeup();
    sleep_debug("Woken back up after %"PRIu32"ms.", since_boot_delta(get_since_boot_ms(), before_time));
    return true;
}
