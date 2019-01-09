#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include "cmd.h"
#include "log.h"
#include "usb.h"
#include "uarts.h"



extern void xPortPendSVHandler( void ) __attribute__ (( naked ));
extern void xPortSysTickHandler( void );
extern void vPortSVCHandler( void ) __attribute__ (( naked ));


void vApplicationStackOverflowHook(xTaskHandle *pxTask,
                                   signed portCHAR *pcTaskName)
{
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


void sv_call_handler(void)
{
    vPortSVCHandler();
}


int main(void)
{
    rcc_clock_setup_hsi(&rcc_hsi_configs[RCC_CLOCK_HSI_48MHZ]);
    uarts_setup();

    systick_interrupt_enable();
    systick_counter_enable();

    platform_raw_msg("----start----");

    log_init();
    cmds_init();
    usb_init();

    vTaskStartScheduler();

    return 0;
}
