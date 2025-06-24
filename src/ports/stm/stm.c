#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <osm/core/platform.h>
#include "flash_data.h"

#include "sos.h"
#include "pinmap.h"
#include <osm/core/log.h>
#include "platform_model.h"
#include <osm/comms/comms.h>
#include <osm/core/sleep.h>
#include <osm/comms/comms_identify.h>

#include <osm/core/adcs.h>


#define ADC_CCR_PRESCALE_1     0x0  /* 0b0000 */
#define ADC_CCR_PRESCALE_2     0x1  /* 0b0001 */
#define ADC_CCR_PRESCALE_4     0x2  /* 0b0010 */
#define ADC_CCR_PRESCALE_6     0x3  /* 0b0011 */
#define ADC_CCR_PRESCALE_8     0x4  /* 0b0100 */
#define ADC_CCR_PRESCALE_10    0x5  /* 0b0101 */
#define ADC_CCR_PRESCALE_12    0x6  /* 0b0110 */
#define ADC_CCR_PRESCALE_16    0x7  /* 0b0111 */
#define ADC_CCR_PRESCALE_32    0x8  /* 0b1000 */
#define ADC_CCR_PRESCALE_64    0x9  /* 0b1001 */
#define ADC_CCR_PRESCALE_128   0xA  /* 0b1010 */
#define ADC_CCR_PRESCALE_256   0xB  /* 0b1011 */
#define ADC_CCR_PRESCALE_MASK  0xF
#define ADC_CCR_PRESCALE_SHIFT 18

static volatile uint32_t since_boot_ms = 0;


static void _stm_setup_systick(void)
{
    systick_set_frequency(1000, rcc_ahb_frequency);
    systick_counter_enable();
    systick_interrupt_enable();
}

static void _stm_setup_rs485(void)
{
    port_n_pins_t re_port_n_pin = OSM_RE_485_PIN;
    port_n_pins_t de_port_n_pin = OSM_DE_485_PIN;

    rcc_periph_clock_enable(OSM_PORT_TO_RCC(re_port_n_pin.port));
    rcc_periph_clock_enable(OSM_PORT_TO_RCC(de_port_n_pin.port));

    gpio_mode_setup(re_port_n_pin.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, re_port_n_pin.pins);
    gpio_mode_setup(de_port_n_pin.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, de_port_n_pin.pins);

    osm_platform_set_rs485_mode(false);
}


static void _stm_adcs_set_prescale(uint32_t adc, uint32_t prescale)
{
    ADC_CCR(adc) &= ~(ADC_CCR_PRESCALE_MASK << ADC_CCR_PRESCALE_SHIFT);
    ADC_CCR(adc) |= (prescale & ADC_CCR_PRESCALE_MASK) << ADC_CCR_PRESCALE_SHIFT;
}


static void _stm_setup_adc_unit(void)
{
    adc_power_off(ADC1);
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    if (ADC_CR(ADC1) & ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    _stm_adcs_set_prescale(ADC1, ADC_CCR_PRESCALE_64);

    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_640DOT5CYC);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);

    adc_calibrate(ADC1);
    adc_set_continuous_conversion_mode(ADC1);
    adc_enable_dma(ADC1);
    adc_power_on(ADC1);
}


static void _stm_setup_adc_dma(adc_setup_config_t* config)
{
    rcc_periph_clock_enable(RCC_DMA1);

    nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);
    dma_set_channel_request(DMA1, DMA_CHANNEL1, 0);

    dma_channel_reset(DMA1, DMA_CHANNEL1);

    dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (uint32_t)&ADC_DR(ADC1));
    dma_set_memory_address(DMA1, DMA_CHANNEL1, config->mem_addr);
    dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
    dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
    dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);
    dma_set_priority(DMA1, DMA_CHANNEL1, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL1);
    dma_set_number_of_data(DMA1, DMA_CHANNEL1, OSM_ADCS_NUM_SAMPLES);
    dma_enable_circular_mode(DMA1, DMA_CHANNEL1);
    dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);

    dma_enable_channel(DMA1, DMA_CHANNEL1);
}


// cppcheck-suppress unusedFunction ; System handler
void hard_fault_handler(void)
{
    /* Special libopencm3 function to handle crashes */
    osm_platform_raw_msg("----big fat crash -----");
    error_state();
}


uint32_t osm_platform_get_frequency(void)
{
    return rcc_ahb_frequency;
}

uint32_t osm_platform_get_hw_id(void)
{
    return DESIG_UNIQUE_ID0;
}


void osm_platform_blink_led_init(void)
{
    gpio_mode_setup(OSM_LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, OSM_LED_PIN);
    gpio_clear(OSM_LED_PORT, OSM_LED_PIN);
}


void osm_platform_blink_led_toggle(void)
{
    gpio_toggle(OSM_LED_PORT, OSM_LED_PIN);
}


void osm_platform_watchdog_reset(void)
{
    iwdg_reset();
}


void osm_platform_watchdog_init(uint32_t ms)
{
    iwdg_set_period_ms(ms);
    iwdg_start();
}


void osm_platform_init(void)
{
    /* Main clocks setup in bootloader, but of course libopencm3 doesn't know. */
    rcc_ahb_frequency = 80e6;
    rcc_apb1_frequency = 80e6;
    rcc_apb2_frequency = 80e6;

    _stm_setup_systick();
    _stm_setup_rs485();

    osm_comms_type_t type = osm_comms_identify();
    if (type == OSM_COMMS_TYPE_UNKNOWN && osm_comms_set_identity())
    {
        osm_log_sys_debug("Failed write COMMS identity");
    }
}


void osm_platform_set_rs485_mode(bool driver_enable)
{
    /* ST3485E
     *
     * 2. RE Receiver output enable. RO is enabled when RE is low; RO is
     * high impedance when RE is high. If RE is high and DE is low, the
     * device will enter a low power shutdown mode.

     * 3. DE Driver output enable. The driver outputs are enabled by
     * bringing DE high. They are high impedance when DE is low. If RE
     * is high DE is low, the device will enter a low-power shutdown
     * mode. If the driver outputs are enabled, the part functions as
     * line driver, while they are high impedance, it functions as line
     * receivers if RE is low.
     *
     * */

    port_n_pins_t re_port_n_pin = OSM_RE_485_PIN;
    port_n_pins_t de_port_n_pin = OSM_DE_485_PIN;
    if (driver_enable)
    {
        osm_modbus_debug("driver:enable receiver:disable");
        gpio_set(re_port_n_pin.port, re_port_n_pin.pins);
        gpio_set(de_port_n_pin.port, de_port_n_pin.pins);
    }
    else
    {
        osm_modbus_debug("driver:disable receiver:enable");
        gpio_clear(re_port_n_pin.port, re_port_n_pin.pins);
        gpio_clear(de_port_n_pin.port, de_port_n_pin.pins);
    }
}


void osm_platform_reset_sys(void)
{
    //osm_comms_power_down();
    //osm_sleep_for_ms(1000);

    SCB_VTOR = FW_ADDR & 0xFFFF;
    /* Initialise master stack pointer. */
    asm volatile("msr msp, %0"::"g"(*(volatile uint32_t *)FW_ADDR));
    /* Jump to application. */
    (*(void (**)())(FW_ADDR + 4))();
}


void osm_platform_hard_reset_sys(void)
{
    scb_reset_system();
}


persist_storage_t* osm_platform_get_raw_persist(void)
{
    return (persist_storage_t*)PERSIST_RAW_DATA;
}


persist_measurements_storage_t* osm_platform_get_measurements_raw_persist(void)
{
    return (persist_measurements_storage_t*)PERSIST_RAW_MEASUREMENTS;
}


bool osm_platform_persist_commit(persist_storage_t* persist_data, persist_measurements_storage_t* persist_measurements)
{
    flash_unlock();
    flash_erase_page(FLASH_CONFIG_PAGE);
    flash_erase_page(FLASH_MEASUREMENTS_PAGE);
    flash_set_data(PERSIST_RAW_DATA, persist_data, sizeof(persist_storage_t));
    flash_set_data(PERSIST_RAW_MEASUREMENTS, persist_measurements, sizeof(persist_measurements_storage_t));
    flash_lock();

    return (memcmp(PERSIST_RAW_DATA, persist_data, sizeof(persist_storage_t)) == 0 &&
            memcmp(PERSIST_RAW_MEASUREMENTS, persist_measurements, sizeof(persist_measurements_storage_t)) == 0);
}

void osm_platform_persist_wipe(void)
{
    flash_unlock();
    flash_erase_page(FLASH_CONFIG_PAGE);
    flash_erase_page(FLASH_MEASUREMENTS_PAGE);
    flash_lock();
}


bool osm_platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page)
{
    flash_unlock();
    flash_erase_page(abs_page);
    flash_program(dst, fw_page, FLASH_PAGE_SIZE);
    flash_lock();

    if (memcmp((void*)dst, fw_page, FLASH_PAGE_SIZE) == 0)
    {
        memset(fw_page, 0xFF, FLASH_PAGE_SIZE);
        return true;
    }

    return false;
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
    flash_clear_status_flags();
}


void osm_platform_start(void)
{
    ;
}


bool osm_platform_running(void)
{
    return true;
}


void osm_platform_deinit(void)
{
}


void osm_platform_setup_adc(adc_setup_config_t* config)
{
    // Setup the clock and gpios
    const port_n_pins_t port_n_pins[] = ADCS_PORT_N_PINS;
    rcc_periph_clock_enable(RCC_ADC1);
    for(unsigned n = 0; n < OSM_ARRAY_SIZE(port_n_pins); n++)
    {
        rcc_periph_clock_enable(OSM_PORT_TO_RCC(port_n_pins[n].port));
        gpio_mode_setup(port_n_pins[n].port,
                        GPIO_MODE_ANALOG,
                        GPIO_PUPD_NONE,
                        port_n_pins[n].pins);
    }
    _stm_setup_adc_unit();
    _stm_setup_adc_dma(config);
}


void osm_platform_adc_set_regular_sequence(uint8_t num_adcs_types, osm_adcs_type_t* adcs_types)
{
    uint8_t channels[ADC_COUNT];
    for (uint8_t i = 0; i < num_adcs_types; i++)
        channels[i] = osm_model_stm_adcs_get_channel(adcs_types[i]);
    adc_set_regular_sequence(ADC1, num_adcs_types, channels);
}


void osm_platform_adc_start_conversion_regular(void)
{
    adc_start_conversion_regular(ADC1);
}


void osm_platform_adc_power_off(void)
{
    adc_power_off(ADC1);
}


// cppcheck-suppress unusedFunction ; System handler
void dma1_channel1_isr(void)  /* ADC1 dma interrupt */
{
    if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL1, DMA_TCIF))
    {
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
        osm_adcs_dma_complete();
    }
}


void osm_platform_adc_set_num_data(unsigned num_data)
{
    dma_disable_channel(DMA1, DMA_CHANNEL1);
    dma_set_number_of_data(DMA1, DMA_CHANNEL1, num_data);
    dma_enable_channel(DMA1, DMA_CHANNEL1);
}


void osm_platform_hpm_enable(bool enable)
{
    port_n_pins_t port_n_pin = OSM_HPM_EN_PIN;
    if (enable)
    {
        rcc_periph_clock_enable(OSM_PORT_TO_RCC(port_n_pin.port));
        gpio_mode_setup(port_n_pin.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, port_n_pin.pins);
        gpio_set(port_n_pin.port, port_n_pin.pins);
    }
    else
    {
        gpio_clear(port_n_pin.port, port_n_pin.pins);
    }
}


void osm_platform_tight_loop(void) {}


void __attribute__((weak)) osm_model_main_loop_iterate(void) {}


void osm_platform_main_loop_iterate(void)
{
    osm_model_main_loop_iterate();
}


uint32_t osm_get_since_boot_ms(void)
{
    return since_boot_ms;
}


// cppcheck-suppress unusedFunction ; System handler
void sys_tick_handler(void)
{
    /* Special libopencm3 function to handle system ticks */
    since_boot_ms++;
}


void osm_platform_gpio_init(const port_n_pins_t * gpio_pin)
{
    rcc_periph_clock_enable(OSM_PORT_TO_RCC(gpio_pin->port));
}


void osm_platform_gpio_setup(const port_n_pins_t * gpio_pin, bool is_input, uint32_t pull)
{
    gpio_mode_setup(gpio_pin->port,
                    (is_input)?GPIO_MODE_INPUT:GPIO_MODE_OUTPUT,
                    pull,
                    gpio_pin->pins);
}

void osm_platform_gpio_set(const port_n_pins_t * gpio_pin, bool is_on)
{
    if (is_on)
        gpio_set(gpio_pin->port, gpio_pin->pins);
    else
        gpio_clear(gpio_pin->port, gpio_pin->pins);
}

bool osm_platform_gpio_get(const port_n_pins_t * gpio_pin)
{
    return gpio_get(gpio_pin->port, gpio_pin->pins) != 0;
}
