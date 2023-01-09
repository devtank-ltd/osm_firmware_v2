#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"
#include "common.h"
#include "io.h"
#include "pulsecount.h"
#include "platform_model.h"


#define PULSECOUNT_COLLECTION_TIME_MS       1000;

#define PULSECOUNT_INSTANCES   {                                       \
    { { MEASUREMENTS_PULSE_COUNT_NAME_1, W1_PULSE_1_IO} ,              \
        W1_PULSE_1_PORT_N_PINS , W1_PULSE_1_EXTI,                      \
        W1_PULSE_1_EXTI_IRQ,                                           \
        IO_SPECIAL_PULSECOUNT_RISING_EDGE,                             \
        0, 0 },                                                        \
    { { MEASUREMENTS_PULSE_COUNT_NAME_2, W1_PULSE_2_IO} ,              \
        W1_PULSE_2_PORT_N_PINS , W1_PULSE_2_EXTI,                      \
        W1_PULSE_2_EXTI_IRQ,                                           \
        IO_SPECIAL_PULSECOUNT_RISING_EDGE,                             \
        0, 0 }                                                         \
}


typedef struct
{
    special_io_info_t   info;
    port_n_pins_t       pnp;
    uint32_t            exti;
    uint8_t             exti_irq;
    io_special_t        edge;
    volatile uint32_t   count;
    uint32_t            send_count;
} pulsecount_instance_t;


static pulsecount_instance_t _pulsecount_instances[] = PULSECOUNT_INSTANCES;


static bool _pulsecount_get_pupd(pulsecount_instance_t* inst, uint8_t* pupd)
{
    if (!inst || !pupd)
    {
        pulsecount_debug("Handed NULL pointer.");
        return false;
    }
    return ios_get_pupd(inst->info.io, pupd);
}


static void _pulsecount_isr(uint32_t ref_port)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        pulsecount_instance_t* inst = &_pulsecount_instances[i];
        /* Check IO is in pulsecount mode. */
        if (!io_is_pulsecount_now(inst->info.io))
            continue;
        /* Ensure the EXTI for pulsecount is the one to be triggered */
        uint32_t exti_state = exti_get_flag_status(inst->exti);
        if (!exti_state)
            continue;
        /* Is the EXTI triggered on the same GPIO port? */
        if (inst->pnp.port != ref_port)
            continue;
        exti_reset_request(inst->exti);
        __sync_add_and_fetch(&inst->count, 1);
    }
}


// cppcheck-suppress unusedFunction ; System handler
void W1_PULSE_1_ISR(void)
{
    port_n_pins_t pulse_1_pnp = W1_PULSE_1_PORT_N_PINS;
    _pulsecount_isr(pulse_1_pnp.port);
}


// cppcheck-suppress unusedFunction ; System handler
void W1_PULSE_2_ISR(void)
{
    port_n_pins_t pulse_2_pnp = W1_PULSE_2_PORT_N_PINS;
    _pulsecount_isr(pulse_2_pnp.port);
}


static void _pulsecount_init_instance(pulsecount_instance_t* instance)
{
    if (!instance)
    {
        pulsecount_debug("Handed NULL instance.");
        return;
    }
    if (!io_is_pulsecount_now(instance->info.io))
    {
        pulsecount_debug("IO is already pulsecount.");
        return;
    }

    uint8_t pupd;
    if (!_pulsecount_get_pupd(instance, &pupd))
    {
        pulsecount_debug("Could not get pull.");
        return;
    }
    pulsecount_debug("PUPD = %"PRIu8, pupd);

    /* Slightly confusing setup due to hardware:
     * NONE:
     *  STM PUPD:   None
     *  HW PUPD:    Down/None
     *  Comments:   This will actually be biased down as the chip pulls
     *              it to ground.
     * UP:
     *  STM PUPD:   None
     *  HW PUPD:    Up
     *  Comments:   As hardware-pull-up is used for pull up, have no pull
     *              up on the STM.
     * DOWN:
     *  STM PUPD:   Down
     *  HW PUPD:    Down/None
     *  Comments:   No comments.
     */
    switch (pupd)
    {
        case IO_PUPD_DOWN:
            break;
        case IO_PUPD_UP:
        case IO_PUPD_NONE:
        default:
            pupd = IO_PUPD_NONE;
            break;
    }

    uint8_t trig;
    switch(instance->edge)
    {
        case IO_SPECIAL_PULSECOUNT_RISING_EDGE:
            trig = EXTI_TRIGGER_RISING;
            break;
        case IO_SPECIAL_PULSECOUNT_FALLING_EDGE:
            trig = EXTI_TRIGGER_FALLING;
            break;
        case IO_SPECIAL_PULSECOUNT_BOTH_EDGE:
            trig = EXTI_TRIGGER_BOTH;
            break;
        default:
            pulsecount_debug("Cannot find a trigger for '%"PRIu8"' pull configuration.", instance->edge);
            return;
    }

    rcc_periph_clock_enable(PORT_TO_RCC(instance->pnp.port));
    gpio_mode_setup(instance->pnp.port, GPIO_MODE_INPUT, pupd, instance->pnp.pins);

    exti_select_source(instance->exti, instance->pnp.port);
    exti_set_trigger(instance->exti, trig);
    exti_enable_request(instance->exti);

    instance->count = 0;
    instance->send_count = 0;

    nvic_enable_irq(instance->exti_irq);
    pulsecount_debug("Pulsecount '%s' enabled", instance->info.name);
}


static void _pulsecount_init(unsigned io, io_special_t edge)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        pulsecount_instance_t* inst = &_pulsecount_instances[i];
        if (inst->info.io == io)
        {
            inst->edge = edge;
            _pulsecount_init_instance(inst);
        }
    }
}


void pulsecount_init(void)
{
}


static void _pulsecount_shutdown_instance(pulsecount_instance_t* instance)
{
    if (!instance)
    {
        pulsecount_debug("Handed NULL instance.");
        return;
    }
    if (io_is_pulsecount_now(instance->info.io))
    {
        pulsecount_debug("IO is not pulsecount.");
        return;
    }
    exti_disable_request(instance->exti);
    nvic_disable_irq(instance->exti_irq);
    instance->count = 0;
    instance->send_count = 0;
    pulsecount_debug("Pulsecount '%s' disabled", instance->info.name);
}


static void _pulsecount_shutdown(unsigned io)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        pulsecount_instance_t* inst = &_pulsecount_instances[i];
        if (inst->info.io == io)
            _pulsecount_shutdown_instance(inst);
    }
}


void pulsecount_enable(unsigned io, bool enable, io_pupd_t pupd, io_special_t edge)
{
    if (enable)
    {
        pulsecount_debug("Initialising pulsecount");
        model_w1_pulse_enable_pupd(io, pupd == IO_PUPD_UP);
        _pulsecount_init(io, edge);
    }
    else
    {
        pulsecount_debug("Shutting-down pulsecount.");
        _pulsecount_shutdown(io);
    }
}


static measurements_sensor_state_t _pulsecount_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = PULSECOUNT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static bool _pulsecount_get_instance(pulsecount_instance_t** instance, char* name)
{
    if (!instance)
    {
        pulsecount_debug("Handed a NULL pointer.");
        return false;
    }
    pulsecount_instance_t* inst;
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        inst = &_pulsecount_instances[i];
        if (strncmp(name, inst->info.name, sizeof(inst->info.name) * sizeof(char)) == 0)
        {
            *instance = inst;
            return true;
        }
    }
    pulsecount_debug("Could not find name in instances.");
    return false;
}


static measurements_sensor_state_t _pulsecount_begin(char* name, bool in_isolation)
{
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!io_is_pulsecount_now(instance->info.io))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    pulsecount_debug("%s at start %"PRIu32, instance->info.name, instance->count);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _pulsecount_get(char* name, measurements_reading_t* value)
{
    if (!value)
    {
        pulsecount_debug("Handed a NULL pointer.");
        return false;
    }
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
    {
        pulsecount_debug("Not an instance.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!io_is_pulsecount_now(instance->info.io))
    {
        pulsecount_debug("IO %s not set up.", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    instance->send_count = instance->count;
    pulsecount_debug("%s at end %"PRIu32, instance->info.name, instance->send_count);
    value->v_i64 = (int64_t)instance->send_count;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static void _pulsecount_ack(char* name)
{
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return;
    pulsecount_debug("%s ack'ed", instance->info.name);
    __sync_sub_and_fetch(&instance->count, instance->send_count);
    instance->send_count = 0;
}


static measurements_value_type_t _pulsecount_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void     pulsecount_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _pulsecount_collection_time;
    inf->init_cb            = _pulsecount_begin;
    inf->get_cb             = _pulsecount_get;
    inf->acked_cb           = _pulsecount_ack;
    inf->value_type_cb      = _pulsecount_value_type;
}


static void hw_pupd_cb(char* args)
{
    char* p;
    unsigned io = strtoul(args, &p, 10);
    if (p == args)
        goto syntax_exit;

    if (io != W1_PULSE_1_IO &&
        io != W1_PULSE_2_IO)
        log_out("Selected IO is not W1 or pulse IO.");

    p = skip_space(p);

    bool enabled;
    if (*p == 'U')
        enabled = true;
    else if (*p == 'D')
        enabled = false;
    else
        goto syntax_exit;

    model_w1_pulse_enable_pupd(io, enabled);
    log_out("IO %u: %"PRIu8, io, (uint8_t)(enabled?1:0));
    return;
syntax_exit:
    log_out("<io> <U/D>");
}


struct cmd_link_t* pulsecount_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "hw_pupd",      "Print all IOs.",           hw_pupd_cb                    , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
