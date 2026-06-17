#pragma once

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"


#include "pinmap.h"


#define NUM_SOS_LOOPS           3


/* SOS in morse code is ... --- ...
* 1. dot is one time unit
* 2. dash is three time units
* 3. space between parts is one time unit
* 4. space between letters is three time units
* 5. space between worss is seven time units.
*/
static void _wait_units(unsigned count)
{
    for(unsigned n = 0; n < clock_get_hz(clk_sys) / 150 * count ; n++)
       asm("nop");
}


static void _led_set(bool on)
{
#ifdef LED_PIN
    gpio_put(LED_PIN, on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#endif
}


static void _dot(void)
{
    _led_set(false);
    _wait_units(1);
    _led_set(true);
}
static void _dash(void)
{
    _led_set(false);
    _wait_units(3);
    _led_set(true);
}

void error_state(void)
{
    uint8_t count = 0;
    while(true)
    {
        for(unsigned n=0; n < 3; n++)
        {
           _dot();
           _wait_units(1);
        }
        _wait_units(3);
        for(unsigned n=0; n < 3; n++)
        {
           _dash();
           _wait_units(1);
        }
        _wait_units(3);
        for(unsigned n=0; n < 3; n++)
        {
           _dot();
           _wait_units(1);
        }
        _wait_units(7);
        count++;
        if (count > NUM_SOS_LOOPS)
            watchdog_reboot(0, 0, 0); /* reset */
    }
}

