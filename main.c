#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include "cmd.h"
#include "log.h"
#include "usb.h"
#include "uarts.h"



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


int main(void)
{
    rcc_clock_setup_pll(&rcc_hse8mhz_configs[RCC_CLOCK_HSE8_72MHZ]);
    uarts_setup();

    platform_raw_msg("----start----");

    log_init();
    cmds_init();
    usb_init();

    vTaskStartScheduler();

    return 0;
}
