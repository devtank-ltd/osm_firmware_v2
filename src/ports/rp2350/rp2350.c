#include <string.h>

#include "pico/unique_id.h"
#include "pico/flash.h"
#include "pico/sync.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/divider.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#include <osm/core/platform.h>
#include <osm/core/log.h>
#include <osm/core/sleep.h>
#include <osm/comms/comms_identify.h>

#include "sos.h"
#include "pinmap.h"
#include "platform_model.h"
#include "model_config.h"


uint32_t osm_platform_get_frequency(void)
{
    return clock_get_hz(clk_sys);
}

uint32_t osm_platform_get_hw_id(void)
{
    pico_unique_board_id_t id = {0};
    pico_get_unique_board_id(&id);
    /* This is an 8 x uint8_t array, but we need to return only 32 bits, so
     * only take last 4.
     */
    uint32_t ret = id.id[7] | (id.id[6] << 8) | (id.id[5] << 8) | (id.id[4] << 8);
    return ret;
}


void osm_platform_blink_led_init(void)
{
#ifdef LED_PIN
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, true);
    gpio_put(LED_PIN, true);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    /* assume cyw43_arch already inited */
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
#else
#pragma message("WARNING: No heartbeat LED defined")
#endif
}


void osm_platform_blink_led_toggle(void)
{
#ifdef LED_PIN
    bool out = gpio_get_out_level(LED_PIN);
    gpio_put(LED_PIN, !out);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    bool out = cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !out);
#endif
}


void osm_platform_watchdog_reset(void)
{
    watchdog_update();
}


void osm_platform_watchdog_init(uint32_t ms)
{
    watchdog_enable(ms, 1);
}


void osm_platform_init(void)
{
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_init();
#endif
    set_sys_clock_khz(150000, true);
}


void osm_platform_set_rs485_mode(bool driver_enable)
{
}


void osm_platform_reset_sys(void)
{
    watchdog_reboot(0, 0, 0);
}


void osm_platform_hard_reset_sys(void)
{
    watchdog_reboot(0, 0, 0);
}


osm_persist_storage_t* osm_platform_get_raw_persist(void)
{
    return (osm_persist_storage_t*)PERSIST_RAW_DATA;
}


osm_persist_measurements_storage_t* osm_platform_get_measurements_raw_persist(void)
{
    return (osm_persist_measurements_storage_t*)PERSIST_RAW_MEASUREMENTS;
}


static bool _rp2350_persist_commit2(uint8_t* dst, uint8_t* src, uint32_t size)
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


bool osm_platform_persist_commit(osm_persist_storage_t* persist_data, osm_persist_measurements_storage_t* persist_measurements)
{
    /* ============================ WARNING ============================
     * Entering critical section, will potentially miss interrupts here
     */
    critical_section_t crit_sec;
    critical_section_init(&crit_sec);
    critical_section_enter_blocking(&crit_sec);
    flash_range_erase(PERSIST_CONFIG_SECTOR, FLASH_SECTOR_SIZE);
    bool ret = (_rp2350_persist_commit2(PERSIST_RAW_DATA, (uint8_t*)persist_data, sizeof(osm_persist_storage_t)) &&
        (_rp2350_persist_commit2(PERSIST_RAW_MEASUREMENTS, (uint8_t*)persist_measurements, sizeof(osm_persist_measurements_storage_t))));
    critical_section_exit(&crit_sec);
    critical_section_deinit(&crit_sec);
    return ret;
}

void osm_platform_persist_wipe(void)
{
    flash_range_erase(PERSIST_CONFIG_SECTOR, FLASH_SECTOR_SIZE);
}


bool osm_platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page)
{
    uintptr_t flash_dst = dst - XIP_BASE;
    if (flash_dst % FLASH_SECTOR_SIZE == 0)
    {
        /* is on a sector boundary, can erase sector */
        flash_range_erase(flash_dst, FLASH_SECTOR_SIZE);
    }
    flash_range_program(flash_dst, fw_page, FLASH_PAGE_SIZE);
    return 0 == memcmp((void*)dst, fw_page, FLASH_PAGE_SIZE);
}


uintptr_t osm_platform_get_fw_addr(unsigned fw_page_index)
{
    return (uintptr_t)(NEW_FW_ADDR + (fw_page_index * FLASH_PAGE_SIZE));
}


void osm_platform_finish_fw(void)
{
}


void osm_platform_clear_flash_flags(void)
{
}


void osm_platform_start(void)
{
}


bool osm_platform_running(void)
{
    return true;
}


void osm_platform_deinit(void)
{
}


void osm_platform_hpm_enable(bool enable)
{
}


void osm_platform_tight_loop(void)
{
    tight_loop_contents();
}


void __attribute__((weak)) model_main_loop_iterate(void) {}


void osm_platform_main_loop_iterate(void)
{
    model_main_loop_iterate();
}


uint32_t osm_get_since_boot_ms(void)
{
    uint64_t t64 = time_us_64();
    t64 /= 1000;
    uint32_t t32 = t64;
    return t32;
}


void osm_platform_gpio_init(const osm_port_n_pins_t * gpio_pin)
{
    gpio_init(gpio_pin->pins);
}


void platform_gpio_setup(const osm_port_n_pins_t * gpio_pin, bool is_input, uint32_t pull)
{
    gpio_init(gpio_pin->pins);
    gpio_set_dir(gpio_pin->pins, !is_input);
    switch(pull)
    {
        case OSM_IO_PUPD_DOWN:
            gpio_pull_down(gpio_pin->pins);
            break;
        case OSM_IO_PUPD_UP:
            gpio_pull_up(gpio_pin->pins);
            break;
        case OSM_IO_PUPD_NONE:
            /* fall through */
        default:
            gpio_disable_pulls(gpio_pin->pins);
            break;
    }
}

void platform_gpio_set(const osm_port_n_pins_t * gpio_pin, bool is_on)
{
    gpio_put(gpio_pin->pins, is_on);
}

bool platform_gpio_get(const osm_port_n_pins_t * gpio_pin)
{
    if (gpio_get_dir(gpio_pin->pins))
    {
        /* is output */
        return gpio_get_out_level(gpio_pin->pins);
    }
    return gpio_get(gpio_pin->pins);
}
