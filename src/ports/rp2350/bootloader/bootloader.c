#include <inttypes.h>
#include <string.h>

#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "hardware/divider.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#include <osm/core/config.h>
#include "pinmap.h"
#include <osm/core/persist_config_header.h>
#include "sos.h"

#if PICO_RP2040
#define VTOR_OFFSET M0PLUS_VTOR_OFFSET
#elif PICO_RP2350
#define VTOR_OFFSET M33_VTOR_OFFSET
#endif

#define FAULT_STR  "----big fat bootloader crash -----"


static void _uart_send_str(const char *s)
{
    uart_inst_t* uart = uart_get_instance(ERR_UART);
    while(*s)
        uart_putc_raw(uart, *s++);
    uart_putc_raw(uart, '\r');
    uart_putc_raw(uart, '\n');
}

void hard_fault_handler(void)
{
    _uart_send_str(FAULT_STR);
    error_state();
}


static void _uart_setup(void)
{
    uart_inst_t* uart = uart_get_instance(ERR_UART);

    uart_init(uart0, 115200);

    gpio_set_function(ERR_UART_TX_PIN, UART_FUNCSEL_NUM(uart, ERR_UART_TX_PIN));
    gpio_set_function(ERR_UART_RX_PIN, UART_FUNCSEL_NUM(uart, ERR_UART_RX_PIN));

    uart_set_format(uart, 8, 1, UART_PARITY_NONE);

    uart_set_hw_flow(uart, false, false);
    uart_set_fifo_enabled(uart, true);
}


static void _uart_unsetup(void)
{
    uart_inst_t* uart = uart_get_instance(ERR_UART);
    uart_deinit(uart);
    gpio_deinit(ERR_UART_TX_PIN);
    gpio_deinit(ERR_UART_TX_PIN);
}


static void _run_application(void)
{
    asm volatile(
      "ldr r0, =%[appcode]\n"
      "ldr r1, =%[vtor]\n"
      "str r0, [r1]\n"
      "ldmia r0, {r0, r1}\n"
      "msr msp, r0\n"
      "bx r1\n"
      :
      : [appcode] "i"(FW_ADDR), [vtor] "i"(PPB_BASE + VTOR_OFFSET)
      :);
}


static void _led_setup(void)
{
#ifdef LED_PIN
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, true);
    gpio_put(LED_PIN, false);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_init()) {
        return;
    }
#else
#pragma message("WARNING: No LED pin defined")
#endif // LED_PIN / CYW43_WL_GPIO_LED_PIN
}


static void _led_unsetup(void)
{
#ifdef LED_PIN
    gpio_deinit(LED_PIN);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_deinit()) {
        return;
    }
#endif // LED_PIN / CYW43_WL_GPIO_LED_PIN
}



static bool _persist_commit2(uint8_t* dst, uint8_t* src, uint32_t size)
{
    static uint8_t  _persist_page[FLASH_PAGE_SIZE];

    divmod_result_t uresult = hw_divider_divmod_u32(size, FLASH_PAGE_SIZE);
    uint32_t num_full_pages = to_quotient_u32(uresult);
    uint32_t last_page_len = to_remainder_u32(uresult);
    uint32_t offset = 0;
    for (uint32_t page = 0; page < num_full_pages; page++)
    {
        flash_range_program((uintptr_t)dst - XIP_BASE + offset, src + offset, FLASH_PAGE_SIZE);
        offset += FLASH_PAGE_SIZE;
    }
    memcpy(_persist_page, src + offset, last_page_len);
    memset(_persist_page + last_page_len, 0xFF, FLASH_PAGE_SIZE - last_page_len);
    flash_range_program((uintptr_t)dst - XIP_BASE + offset, _persist_page, FLASH_PAGE_SIZE);
    return memcmp(dst, src, size) == 0;
}


static bool _persist_commit(persist_storage_t* persist_data, persist_measurements_storage_t* persist_measurements)
{
    flash_range_erase(PERSIST_CONFIG_SECTOR, FLASH_SECTOR_SIZE);
    return (_persist_commit2(PERSIST_RAW_DATA, (uint8_t*)persist_data, sizeof(persist_storage_t)) &&
        (_persist_commit2(PERSIST_RAW_MEASUREMENTS, (uint8_t*)persist_measurements, sizeof(persist_measurements_storage_t))));
}


int main(void)
{
    _uart_setup();
    _uart_send_str("Bootloader");
    _led_setup();

    persist_storage_t * config = (persist_storage_t*)PERSIST_RAW_DATA;

    if (config->pending_fw && config->pending_fw != 0xFFFFFFFF)
    {
        _uart_send_str("New Firmware Flag");
        if ((*(uint32_t*)(uintptr_t)NEW_FW_ADDR)!=0xFFFFFFFF)
        {
            uint32_t size = config->pending_fw;
            unsigned sector_count = size / FLASH_SECTOR_SIZE;
            if (size % FLASH_PAGE_SIZE)
                sector_count++;
            uint32_t w_sector = FW_SECTOR;
            uint32_t r_addr = NEW_FW_ADDR;
            uint32_t w_addr = FW_ADDR;
            for (unsigned sector_no = 0; sector_no < sector_count; sector_no++)
            {
                unsigned retries = 3;
                while (retries--)
                {
                    static uint8_t _sector[FLASH_SECTOR_SIZE];
                    flash_range_erase(w_sector, FLASH_SECTOR_SIZE);
                    memcpy(_sector, (uint8_t*)r_addr, FLASH_SECTOR_SIZE);
                    flash_range_program(w_sector, _sector, FLASH_SECTOR_SIZE);
                    if (memcmp((void*)w_addr, (uint8_t*)r_addr, FLASH_SECTOR_SIZE) == 0)
                    {
                        _uart_send_str("FW page copied");
                        break;
                    }
                    if (retries)
                    {
                        _uart_send_str("ERROR: FW page copy failed, trying again.");
                    }
                    else
                    {
                        _uart_send_str("ERROR: FW page copy failed all retries.");
                        error_state();
                    }
                }
                w_sector += FLASH_SECTOR_SIZE;
                r_addr += FLASH_SECTOR_SIZE;
                w_addr += FLASH_SECTOR_SIZE;
            }
            _uart_send_str("New Firmware Copied");
            if (memcmp((const void*)FW_ADDR, (const void*)NEW_FW_ADDR, size) != 0)
            {
                _uart_send_str("ERROR: New Firmware check failed.");
                error_state();
            }
        }
        else _uart_send_str("No New Firmware!");

        static persist_storage_t _config = {0};
        _config = *config;
        static persist_measurements_storage_t _measurements = {0};
        _measurements = *(persist_measurements_storage_t*)PERSIST_RAW_MEASUREMENTS;
        _config.pending_fw = 0;
        if (!_persist_commit(&_config, &_measurements))
        {
            _uart_send_str("WARNING: Config update failed.");
        }
    }

    if ((*(uint32_t*)(uintptr_t)FW_ADDR)==0xFFFFFFFF)
    {
        _uart_send_str("ERROR: No Firmware!");
        error_state();
    }
    else {
        uart_default_tx_wait_blocking();
        _uart_unsetup();
        _led_unsetup();
        _run_application();
    }

    return 0;
}
