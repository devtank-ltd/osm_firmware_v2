#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "io.h"
#include "io_watch.h"
#include "config.h"
#include "platform.h"
#include "log.h"
#include "platform_model.h"
#include "sleep.h"



typedef struct
{
    unsigned        io;
    port_n_pins_t   pnp;
    uint32_t        exti;
    uint8_t         exti_irq;
} io_watch_instance_t;


const unsigned              ios_watch_ios[IOS_WATCH_COUNT]                  = IOS_WATCH_IOS;
static measurements_def_t*  _ios_watch_measurements_def[IOS_WATCH_COUNT];
static measurements_data_t* _ios_watch_measurements_data[IOS_WATCH_COUNT];


static io_watch_instance_t _io_watch_instances[IOS_WATCH_COUNT] =
{
    {
        .io         = W1_PULSE_1_IO,
        .pnp        = W1_PULSE_1_PORT_N_PINS,
        .exti       = W1_PULSE_1_EXTI,
        .exti_irq   = W1_PULSE_1_EXTI_IRQ,
    },
    {
        .io         = W1_PULSE_2_IO,
        .pnp        = W1_PULSE_2_PORT_N_PINS,
        .exti       = W1_PULSE_2_EXTI,
        .exti_irq   = W1_PULSE_2_EXTI_IRQ,
    }
};


static io_watch_instance_t* _io_watch_get_inst(unsigned io)
{
    for (unsigned i = 0; i < IOS_WATCH_COUNT; i++)
    {
        if (_io_watch_instances[i].io == io)
            return &_io_watch_instances[i];
    }
    return NULL;
}


static bool _io_watch_enable2(io_watch_instance_t* inst, bool enabled, io_pupd_t pupd)
{
    if (enabled)
    {
        uint8_t pupd8;
        switch (pupd)
        {
            case IO_PUPD_DOWN:
                pupd8 = GPIO_PUPD_PULLDOWN;
                break;
            case IO_PUPD_UP:
                pupd8 = GPIO_PUPD_PULLUP;
                break;
            case IO_PUPD_NONE:
                pupd8 = GPIO_PUPD_NONE;
                break;
            default:
                io_debug("Could not determine PUPD.");
                return false;
        }

        model_setup_pulse_pupd(&pupd8);

        rcc_periph_clock_enable(PORT_TO_RCC(inst->pnp.port));
        gpio_mode_setup(inst->pnp.port, GPIO_MODE_INPUT, pupd8, inst->pnp.pins);

        exti_select_source(inst->exti, inst->pnp.port);
        exti_set_trigger(inst->exti, EXTI_TRIGGER_BOTH);
        exti_enable_request(inst->exti);

        nvic_enable_irq(inst->exti_irq);

        io_debug("Set up IO %u for IO watch.", inst->io);
        return true;
    }
    /* Remove interrupt etc. */
    exti_disable_request(inst->exti);
    nvic_disable_irq(inst->exti_irq);
    io_debug("IO watch for IO %u disabled", inst->io);
    return true;
}


bool io_watch_enable(unsigned io, bool enabled, io_pupd_t pupd)
{
    io_watch_instance_t* inst = _io_watch_get_inst(io);
    if (!inst)
    {
        io_debug("Could not get the instance for '%u'", io);
        return false;
    }
    model_w1_pulse_enable_pupd(io, pupd == IO_PUPD_UP);
    return _io_watch_enable2(inst, enabled, pupd);
}


void io_watch_isr(uint32_t exti_group)
{
    for (unsigned i = 0; i < IOS_WATCH_COUNT; i++)
    {
        io_watch_instance_t* inst = &_io_watch_instances[i];
        if (!inst)
            continue;

        if (!io_is_watch_now(inst->io))
            continue;

        uint32_t exti_state = exti_get_flag_status(inst->exti);
        if(!exti_state)
            continue;

        exti_reset_request(inst->exti);

        measurements_def_t* def = _ios_watch_measurements_def[i];
        measurements_data_t* data = _ios_watch_measurements_data[i];
        if (!def || !data)
            continue;

        if (!def->interval || !def->samplecount)
            continue;

        data->instant_send = 1;
        sleep_exit_sleep_mode();
    }
}


void io_watch_init(void)
{
    /* Must be called after measurements_init */
    char name[MEASURE_NAME_NULLED_LEN] = IOS_MEASUREMENT_NAME_PRE;
    unsigned len = strnlen(name, MEASURE_NAME_LEN);
    char* p = name + len;
    for (unsigned i = 0; i < IOS_WATCH_COUNT; i++)
    {
        snprintf(p, MEASURE_NAME_NULLED_LEN - len, "%02u", ios_watch_ios[i]);
        if (!measurements_get_measurements_def(name, &_ios_watch_measurements_def[i], &_ios_watch_measurements_data[i]))
        {
            _ios_watch_measurements_def[i] = NULL;
            _ios_watch_measurements_data[i] = NULL;
            io_debug("Could not find measurements data for '%s'", name);
        }
    }

    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
    {
        if (io_is_watch_now(n))
        {
            uint8_t pupd;
            if (!ios_get_pupd(n, &pupd))
            {
                io_debug("Could not get pull of IO %u", n);
                continue;
            }
            if (!io_watch_enable(n, true, pupd))
                io_debug("Could not enable IO watch for IO %u on init.", n);
        }
    }
}
