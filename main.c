#include <stdint.h>
#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>

#include "pinmap.h"
#include "cmd.h"
#include "log.h"
#include "uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "timers.h"
#include "io.h"
#include "uart_rings.h"
#include "persist_config.h"


void hard_fault_handler(void)
{
    platform_raw_msg("----big fat crash -----");
    while(true);
}


int main(void)
{
    rcc_osc_on(RCC_MSI);
    rcc_set_msi_range(RCC_CR_MSIRANGE_800KHZ);
    rcc_set_hpre(RCC_CFGR_HPRE_NODIV);
    rcc_set_ppre1(RCC_CFGR_PPRE1_NODIV);
    rcc_set_ppre2(RCC_CFGR_PPRE2_NODIV);

    flash_prefetch_enable();
    flash_set_ws(FLASH_ACR_LATENCY_1WS);

    rcc_set_sysclk_source(RCC_MSI);

    rcc_apb1_frequency = 80000000;
    rcc_ahb_frequency = 80000000;

    uarts_setup();

    platform_raw_msg("----start----");
    log_sys_debug("Frequency : %lu", rcc_ahb_frequency);
    log_sys_debug("Version : %s", GIT_VERSION);

    log_init();
    init_persistent();
    cmds_init();
    ios_init();

    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);

    log_sys_debug("Press 'D' for debug.");
    log_async_log = true;

    while(true)
    {
        for(unsigned n = 0; n < rcc_ahb_frequency / 1000; n++)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
        }

        gpio_toggle(LED_PORT, LED_PIN);
    }

    return 0;
}
