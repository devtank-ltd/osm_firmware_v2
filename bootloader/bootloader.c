#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/scb.h>

#define IOS_COUNT 11

#include "config.h"
#include "pinmap.h"
#include "persist_config_header.h"
#include "flash_data.h"
#include "sos.h"

#define FAULT_STR  "----big fat bootloader crash -----"

static persist_storage_t _config;


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
    error_state();
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

    rcc_osc_on(RCC_LSI);
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

    uart_send_str("Bootloader");

    rcc_periph_clock_enable(PORT_TO_RCC(LED_PORT));
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);

    persist_storage_t * config = (persist_storage_t*)PERSIST_RAW_DATA;

    if (config->pending_fw && config->pending_fw != 0xFFFFFFFF)
    {
        uart_send_str("New Firmware Flag");
        if ((*(uint32_t*)(uintptr_t)NEW_FW_ADDR)!=0xFFFFFFFF)
        {
            uint32_t size = config->pending_fw;
            unsigned page_count = size / FLASH_PAGE_SIZE;
            if (size % FLASH_PAGE_SIZE)
                page_count++;
            unsigned fw_first_page = (FW_ADDR - FLASH_ADDRESS) / FLASH_PAGE_SIZE;
            unsigned fw_end_page = fw_first_page + page_count;
            uint8_t * dst = (uint8_t*)FW_ADDR;
            uint8_t * src = (uint8_t*)NEW_FW_ADDR;
            for(unsigned n = fw_first_page; n < fw_end_page; n++)
            {
                unsigned retries = 3;
                while(retries--)
                {
                    flash_unlock();
                    flash_erase_page(n);
                    flash_program((uintptr_t)dst, src, FLASH_PAGE_SIZE);
                    flash_lock();
                    if (memcmp((void*)dst, src, FLASH_PAGE_SIZE) == 0)
                    {
                        uart_send_str("FW page copied");
                        break;
                    }

                    if (retries)
                    {
                        uart_send_str("ERROR: FW page copy failed, trying again.");
                        flash_clear_status_flags();
                    }
                    else
                    {
                        uart_send_str("ERROR: FW page copy failed all retries.");
                        error_state();
                    }
                }
                src += FLASH_PAGE_SIZE;
                dst += FLASH_PAGE_SIZE;
            }
            uart_send_str("New Firmware Copied");
            if (memcmp((const void*)FW_ADDR, (const void*)NEW_FW_ADDR, size) != 0)
            {
                uart_send_str("ERROR: New Firmware check failed.");
                error_state();
            }
        }
        else uart_send_str("No New Firmware!");

        _config = *config;
        _config.pending_fw = 0;
        flash_unlock();
        flash_erase_page(FLASH_CONFIG_PAGE);
        flash_set_data(config, &_config, sizeof(_config));
        flash_lock();
        if (memcmp((const void*)config, (const void*)&_config, sizeof(_config)) != 0)
            uart_send_str("WARNING: Config update failed.");
    }

    if ((*(uint32_t*)(uintptr_t)FW_ADDR)==0xFFFFFFFF)
    {
        uart_send_str("ERROR: No Firmware!");
        error_state();
    }
    else run_firmware(FW_ADDR);

    return 0;
}
