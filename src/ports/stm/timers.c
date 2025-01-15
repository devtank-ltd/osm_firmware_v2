#include <stdlib.h>

#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include <osm/core/config.h>
#include <osm/core/timers.h>
#include <osm/core/log.h>
#include <osm/core/basetypes.h>
#include <osm/core/common.h>


static volatile uint32_t us_counter = 0;


void osm_timer_delay_us(uint16_t wait_us)
{
    TIM_ARR(TIM2) = wait_us;
    TIM_EGR(TIM2) = TIM_EGR_UG;
    //TIM_CR1(TIM2) |= TIM_CR1_CEN;
    timer_enable_counter(TIM2);
    while (TIM_CR1(TIM2) & TIM_CR1_CEN);
}


void osm_timer_delay_us_64(uint64_t wait_us)
{
    while (wait_us > UINT16_MAX)
    {
        osm_timer_delay_us(UINT16_MAX);
        wait_us -= UINT16_MAX;
    }
    osm_timer_delay_us(wait_us);
}


void     osm_timers_init()
{
    rcc_periph_clock_enable(RCC_TIM2);

    timer_disable_counter(TIM2);

    timer_set_mode(TIM2,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow
    timer_set_prescaler(TIM2, rcc_ahb_frequency / 1000000-1);
    timer_set_period(TIM2, 0xffff);
    timer_enable_preload(TIM2);
    timer_one_shot_mode(TIM2);
}


static command_response_t _cmd_timer_cb(char* args, cmd_ctx_t * ctx)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    cmd_ctx_out(ctx,"Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
    return COMMAND_RESP_OK;
}


struct cmd_link_t* timers_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "timer",        "Test usecs timer",         _cmd_timer_cb                  , false , NULL},
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
