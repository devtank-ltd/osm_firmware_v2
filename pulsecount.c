#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"
#include "sys_time.h"



static volatile uint32_t _pulsecount      = 0;
static uint32_t          _send_pulsecount = 0;
static bool              _enabled         = false;


void PULSE_ISR(void)
{
    exti_reset_request(PULSE_EXTI);
    _pulsecount++;
}


void pulsecount_init(void)
{
    if (!_enabled)
        return;
    rcc_periph_clock_enable(PORT_TO_RCC(PULSE_PORT));

    gpio_mode_setup(PULSE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, PULSE_PIN);

    exti_select_source(PULSE_EXTI, PULSE_PORT);
    exti_set_trigger(PULSE_EXTI, EXTI_TRIGGER_FALLING);
    exti_enable_request(PULSE_EXTI);

    nvic_enable_irq(PULSE_EXTI_IRQ);
}


static void _pulsecount_shutdown(void)
{
    if (_enabled)
        return;
    exti_disable_request(PULSE_EXTI);
    nvic_disable_irq(PULSE_EXTI_IRQ);
    _pulsecount = 0;
}


void     pulsecount_enable(bool enable)
{
    _enabled = enable;
    if (_enabled)
        pulsecount_init();
    else
        _pulsecount_shutdown();
}


void pulsecount_log()
{
    log_out("pulsecount : %"PRIu32, _pulsecount);
}


uint32_t pulsecount_collection_time(void)
{
    return 1000;
}


bool     pulsecount_begin(char* name)
{
    if (!_enabled)
        return false;
    pulsecount_debug("pulsecount at start %"PRIu32, _pulsecount);
    return true;
}


bool     pulsecount_get(char* name, value_t* value)
{
    if (!_enabled || !value)
        return false;

    _send_pulsecount = _pulsecount;
    pulsecount_debug("pulsecount at end %"PRIu32, _send_pulsecount);
    *value = value_from(_send_pulsecount);
    return true;
}


void     pulsecount_ack(void)
{
    __sync_sub_and_fetch(&_pulsecount, _send_pulsecount);
    _send_pulsecount = 0;
}
