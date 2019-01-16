#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>


#include "cmd.h"
#include "log.h"
#include "usb.h"
#include "uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "timers.h"

extern void xPortPendSVHandler( void ) __attribute__ (( naked ));
extern void xPortSysTickHandler( void );


void
vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName) {
    (void)pxTask;
    (void)pcTaskName;
    platform_raw_msg("----big fat FreeRTOS crash -----");
    while(true);
}


void hard_fault_handler(void)
{
    platform_raw_msg("----big fat libopen3 crash -----");
    while(true);
}


void pend_sv_handler(void)
{
    xPortPendSVHandler();
}

void sys_tick_handler(void)
{
    xPortSysTickHandler();
}


int main(void) {
    rcc_clock_setup_in_hsi48_out_48mhz();
    uarts_setup();

    platform_raw_msg("----start----");
    log_out("Frequency : %lu", rcc_ahb_frequency);

    log_init();
    cmds_init();
    usb_init();
    adcs_init();
    pulsecount_init();
    timers_init();

    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    gpio_clear(GPIOA, GPIO5);

    log_async_log = true;

    vTaskStartScheduler();

    return 0;
}
