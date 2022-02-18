#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"
#include "common.h"
#include "io.h"
#include "pulsecount.h"


#define PULSECOUNT_COLLECTION_TIME_MS       1000;


static volatile uint32_t _pulsecount      = 0;
static uint32_t          _send_pulsecount = 0;


void PULSE_ISR(void)
{
    exti_reset_request(PULSE_EXTI);
    __sync_add_and_fetch(&_pulsecount, 1);
}


void pulsecount_init(void)
{
    if (!io_is_special_now(PULSE_IO))
        return;
    rcc_periph_clock_enable(PORT_TO_RCC(PULSE_PORT));

    gpio_mode_setup(PULSE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, PULSE_PIN);

    exti_select_source(PULSE_EXTI, PULSE_PORT);
    exti_set_trigger(PULSE_EXTI, EXTI_TRIGGER_FALLING);
    exti_enable_request(PULSE_EXTI);

    _pulsecount = 0;
    _send_pulsecount = 0;

    nvic_enable_irq(PULSE_EXTI_IRQ);
    pulsecount_debug("Pulsecount enabled");
}


static void _pulsecount_shutdown(void)
{
    if (io_is_special_now(PULSE_IO))
        return;
    exti_disable_request(PULSE_EXTI);
    nvic_disable_irq(PULSE_EXTI_IRQ);
    _pulsecount = 0;
    _send_pulsecount = 0;
    pulsecount_debug("Pulsecount disabled");
}


void     pulsecount_enable(bool enable)
{
    if (enable)
        pulsecount_init();
    else
        _pulsecount_shutdown();
}


void pulsecount_log()
{
    log_out("pulsecount : %"PRIu32, _pulsecount);
}


measurements_sensor_state_t pulsecount_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = PULSECOUNT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t pulsecount_begin(char* name)
{
    if (!io_is_special_now(PULSE_IO))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    pulsecount_debug("pulsecount at start %"PRIu32, _pulsecount);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t pulsecount_get(char* name, value_t* value)
{
    if (!io_is_special_now(PULSE_IO) && !value)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    _send_pulsecount = _pulsecount;
    pulsecount_debug("pulsecount at end %"PRIu32, _send_pulsecount);
    *value = value_from(_send_pulsecount);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void     pulsecount_ack(void)
{
    pulsecount_debug("Pulsecount ack'ed");
    __sync_sub_and_fetch(&_pulsecount, _send_pulsecount);
    _send_pulsecount = 0;
}
