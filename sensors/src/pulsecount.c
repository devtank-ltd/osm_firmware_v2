#include <inttypes.h>
#include <string.h>

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

#define PULSECOUNT_INSTANCES   {                                       \
    { { MEASUREMENTS_PULSE_COUNT_NAME_1, W1_PULSE_1_IO} ,              \
        W1_PULSE_1_PIN  , W1_PULSE_1_EXTI,                             \
        W1_PULSE_1_EXTI_IRQ,                                           \
        W1_PULSE_1_TIM, W1_PULSE_1_TIM_IRQ, W1_PULSE_1_TIM_RCC ,       \
        0, 0 },                                                        \
    { { MEASUREMENTS_PULSE_COUNT_NAME_2, W1_PULSE_2_IO} ,              \
        W1_PULSE_2_PIN , W1_PULSE_2_EXTI,                              \
        W1_PULSE_2_EXTI_IRQ,                                           \
        W1_PULSE_2_TIM, W1_PULSE_2_TIM_IRQ, W1_PULSE_2_TIM_RCC ,       \
        0, 0 }                                                         \
}

#define PULSECOUNT_EDGE_COOLDOWN_MS          50


typedef struct
{
    special_io_info_t   info;
    uint16_t            pin;
    uint32_t            exti;
    uint8_t             exti_irq;
    uint32_t            tim;
    uint8_t             tim_irq;
    uint32_t            tim_rcc;
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


// cppcheck-suppress unusedFunction ; System handler
void W1_PULSE_ISR(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        pulsecount_instance_t* inst = &_pulsecount_instances[i];
        if (!io_is_pulsecount_now(inst->info.io))
            continue;
        uint32_t exti_state = exti_get_flag_status(inst->exti);
        if (!exti_state)
            continue;
        exti_reset_request(inst->exti);
        exti_disable_request(inst->exti);
        __sync_add_and_fetch(&inst->count, 1);
        timer_set_counter(inst->tim, 0);
        timer_enable_counter(inst->tim);
    }
}


static void _pulsecount_init_instance(pulsecount_instance_t* instance)
{
    if (!instance)
        return;
    if (!io_is_pulsecount_now(instance->info.io))
        return;


    uint8_t pupd;
    if (!_pulsecount_get_pupd(instance, &pupd))
        return;

    uint8_t trig;
    switch(pupd)
    {
        case GPIO_PUPD_PULLDOWN:
            trig = EXTI_TRIGGER_RISING;
            break;
        case GPIO_PUPD_PULLUP:
            trig = EXTI_TRIGGER_FALLING;
            break;
        case GPIO_PUPD_NONE:
            trig = EXTI_TRIGGER_FALLING;
            break;
        default:
            pulsecount_debug("Cannot find a trigger for '%"PRIu8"' pull configuration.", pupd);
            return;
    }

    rcc_periph_clock_enable(PORT_TO_RCC(W1_PULSE_PORT));
    gpio_mode_setup(W1_PULSE_PORT, GPIO_MODE_INPUT, pupd, instance->pin);

    exti_select_source(instance->exti, W1_PULSE_PORT);
    exti_set_trigger(instance->exti, trig);
    exti_enable_request(instance->exti);

    instance->count = 0;
    instance->send_count = 0;

    nvic_enable_irq(instance->exti_irq);

    /* Set up timer */
    rcc_periph_clock_enable(instance->tim_rcc);

    timer_disable_counter(instance->tim);

    timer_set_mode(instance->tim,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);

    timer_set_prescaler(instance->tim, rcc_ahb_frequency / 10000 - 1);//-1 because it starts at zero, and interrupts on the overflow
    timer_set_period(instance->tim, PULSECOUNT_EDGE_COOLDOWN_MS * 10);
    timer_enable_preload(instance->tim);
    timer_one_shot_mode(instance->tim);
    timer_enable_irq(instance->tim, TIM_DIER_CC1IE);

    nvic_enable_irq(instance->tim_irq);
    nvic_set_priority(instance->tim_irq, 1);

    pulsecount_debug("Pulsecount '%s' enabled", instance->info.name);
}


void pulsecount_init(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        _pulsecount_init_instance(&_pulsecount_instances[i]);
    }
}


void W1_PULSE_1_TIM_ISR(void)
{
    timer_clear_flag(W1_PULSE_1_TIM, TIM_SR_CC1IF);
    pulsecount_instance_t* inst = &_pulsecount_instances[0];
    exti_enable_request(inst->exti);
    timer_disable_counter(inst->tim);
}


void W1_PULSE_2_TIM_ISR(void)
{
    timer_clear_flag(W1_PULSE_2_TIM, TIM_SR_CC1IF);
    pulsecount_instance_t* inst = &_pulsecount_instances[1];
    exti_enable_request(inst->exti);
    timer_disable_counter(inst->tim);
}


static void _pulsecount_shutdown_instance(pulsecount_instance_t* instance)
{
    if (!instance)
        return;
    if (io_is_pulsecount_now(instance->info.io))
        return;
    exti_disable_request(instance->exti);
    nvic_disable_irq(instance->exti_irq);
    instance->count = 0;
    instance->send_count = 0;
    pulsecount_debug("Pulsecount disabled");
}


static void _pulsecount_shutdown(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        _pulsecount_shutdown_instance(&_pulsecount_instances[i]);
    }
}


void pulsecount_enable(bool enable)
{
    if (enable)
        pulsecount_init();
    else
        _pulsecount_shutdown();
}


void _pulsecount_log_instance(pulsecount_instance_t* instance)
{
    if (!io_is_pulsecount_now(instance->info.io))
        return;
    log_out("%s : %"PRIu32, instance->info.name, instance->count);
}


void pulsecount_log()
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        _pulsecount_log_instance(&_pulsecount_instances[i]);
    }
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


measurements_sensor_state_t pulsecount_begin(char* name)
{
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!io_is_pulsecount_now(instance->info.io))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    pulsecount_debug("%s at start %"PRIu32, instance->info.name, instance->count);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t pulsecount_get(char* name, value_t* value)
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
    *value = value_from(instance->send_count);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void pulsecount_ack(char* name)
{
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return;
    pulsecount_debug("%s ack'ed", instance->info.name);
    __sync_sub_and_fetch(&instance->count, instance->send_count);
    instance->send_count = 0;
}
