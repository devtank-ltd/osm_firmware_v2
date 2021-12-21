#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/scb.h>

#include "config.h"
#include "pinmap.h"
#include "persist_config_header.h"

#define FAULT_STR  "----big fat bootloader crash -----"

static void uart_send_str(char *s)
{
    while(*s)
        usart_send_blocking(ERR_UART, *s++);
    usart_send_blocking(ERR_UART, '\r');
    usart_send_blocking(ERR_UART, '\n');
}

void hard_fault_handler(void)
{
    uart_send_str(FAULT_STR);
    while(true);
}


static void uart_setup(void)
{
    rcc_periph_clock_enable(ERR_UART_RCC);
    rcc_periph_clock_enable(PORT_TO_RCC(ERR_UART_GPIO));

    gpio_mode_setup(ERR_UART_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, ERR_UART_PINS);
    gpio_set_af(ERR_UART_GPIO, ERR_UART_GPIO_AF, ERR_UART_PINS);

    usart_set_baudrate(ERR_UART, 115200);
    usart_set_databits(ERR_UART, 8);
    usart_set_stopbits(ERR_UART, USART_STOPBITS_1);
    usart_set_mode(ERR_UART, USART_MODE_TX);
    usart_set_parity(ERR_UART, USART_PARITY_NONE);
    usart_set_flow_control(ERR_UART, USART_FLOWCONTROL_NONE);
    usart_enable(ERR_UART);
}


static void clock_setup(void)
{
    rcc_osc_on(RCC_HSI16);

    flash_prefetch_enable();
    flash_set_ws(4);
    flash_dcache_enable();
    flash_icache_enable();
    /* 16MHz / 4(M) = > 4 * 40(N) = 160MHz VCO => 80MHz main pll  */
    rcc_set_main_pll(RCC_PLLCFGR_PLLSRC_HSI16, 4, 40,
                     0, 0, RCC_PLLCFGR_PLLR_DIV2);
    rcc_osc_on(RCC_PLL);
    rcc_wait_for_osc_ready(RCC_PLL);

    /* Enable clocks for peripherals we need */
    rcc_periph_clock_enable(RCC_SYSCFG);

    rcc_set_sysclk_source(RCC_CFGR_SW_PLL); /* careful with the param here! */
    rcc_wait_for_sysclk_status(RCC_PLL);
    /* FIXME - eventually handled internally */
    rcc_ahb_frequency = 80e6;
    rcc_apb1_frequency = 80e6;
    rcc_apb2_frequency = 80e6;
}


static void run_firmware(uintptr_t addr)
{
    SCB_VTOR = addr & 0xFFFF;
    /* Initialise master stack pointer. */
    asm volatile("msr msp, %0"::"g"(*(volatile uint32_t *)addr));
    /* Jump to application. */
    (*(void (**)())(addr + 4))();
}



int main(void)
{
    clock_setup();
    uart_setup();

    rcc_periph_clock_enable(PORT_TO_RCC(LED_PORT));
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);

    persist_storage_t * config = (persist_storage_t*)PERSIST__RAW_DATA;

    run_firmware(config->fw_a);

    return 0;
}
