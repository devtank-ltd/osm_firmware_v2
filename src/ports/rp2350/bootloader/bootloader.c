#include <inttypes.h>
#include <string.h>

#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/flash.h"

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


static void run_application(void)
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


int main(void)
{
    _uart_setup();

    _uart_send_str("Bootloader");

#ifdef LED_PIN
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, true);
    gpio_put(LED_PIN, false);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_init()) {
        return -1;
    }
#else
#pragma message("WARNING: No LED pin defined")
#endif // LED_PIN / CYW43_WL_GPIO_LED_PIN

    persist_storage_t * config = (persist_storage_t*)PERSIST_RAW_DATA;

    if (config->pending_fw && config->pending_fw != 0xFFFFFFFF)
    {
        _uart_send_str("New Firmware Flag");
    }

    if ((*(uint32_t*)(uintptr_t)FW_ADDR)==0xFFFFFFFF)
    {
        _uart_send_str("ERROR: No Firmware!");
        error_state();
    }
    else {
        run_application();
    }

    return 0;
}
