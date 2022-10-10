#pragma once


#include <libopencm3/cm3/scb.h>

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
    for(unsigned n = 0; n < rcc_ahb_frequency / 150 * count ; n++)
       asm("nop");
}
static void _dot(void)
{
    gpio_clear(LED_PORT, LED_PIN);
    _wait_units(1);
    gpio_set(LED_PORT, LED_PIN);
}
static void _dash(void)
{
    gpio_clear(LED_PORT, LED_PIN);
    _wait_units(3);
    gpio_set(LED_PORT, LED_PIN);
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
            scb_reset_system(); /* reset */
    }
}

