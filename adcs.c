#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "sys_time.h"


#define ADC_CCR_PRESCALE_MASK  (15 << 18)
#define ADC_CCR_PRESCALE_64    ( 9 << 18)


#define ADCS_CONFIG_PRESCALE        ADC_CCR_PRESCALE_64
#define ADCS_CONFIG_SAMPLE_TIME     ADC_SMPR_SMP_640DOT5CYC


#define ADCS_DEFAULT_COLLECTION_TIME    2000;


#define ADCS_NUM_SAMPLES            480
#define ADCS_TIMEOUT_TIME_MS        5000


typedef enum
{
    ADCS_VAL_STATUS_IDLE,
    ADCS_VAL_STATUS_DOING,
    ADCS_VAL_STATUS_DONE,
} adc_value_status_t;


typedef enum
{
    ADCS_ADC_INDEX_ADC1    = 0 ,
    ADCS_ADC_INDEX_ADC2    = 1 ,
} adcs_adc_index_enum_t;


typedef enum
{
    ADCS_CHAN_INDEX_CURRENT_CLAMP  = 0 ,
    ADCS_CHAN_INDEX_BAT_MON        = 1 ,
    ADCS_CHAN_INDEX_3V3_MON        = 2 ,
    ADCS_CHAN_INDEX_5V_MON         = 3 ,
} adcs_chan_index_enum_t;


typedef struct
{
    uint32_t              adc_unit;
    uint32_t              dma_unit;
    uint32_t              dma_rcc;
    uint8_t               dma_irqn;
    uint8_t               dma_channel;
    uint8_t               priority;
    uint8_t               enabled;
} adc_dma_channel_t;


typedef struct
{
    uint64_t sum;
    uint32_t count;
    uint16_t min;
    uint16_t max;
} adc_simple_val_t;


static bool                         adcs_running                                        = false;
static adc_simple_val_t             adcs_val;
static uint8_t                      adc_channel_array[ADC_COUNT]                        = ADC_CHANNELS;
static uint16_t                     midpoint;
static volatile adc_value_status_t  adc_value_status                                    = ADCS_VAL_STATUS_IDLE;


uint32_t adcs_collection_time(void)
{
    /**
    Could calculate how long it should take to get the results. For now use 2 seconds.
    */
    return ADCS_DEFAULT_COLLECTION_TIME;
}


static bool _adcs_set_prescale(uint32_t adc, uint32_t prescale)
{
    if (((ADC_CR(adc) & ADC_CR_ADCAL    )==0) &&
        ((ADC_CR(adc) & ADC_CR_JADSTART )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADSTART  )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADSTP    )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADDIS    )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADEN     )==0) )
    {
        ADC_CCR(adc) = (ADC_CCR(adc) & ADC_CCR_PRESCALE_MASK) | prescale;
        return true;
    }
    return false;
}


static void _adcs_setup_adc(void)
{
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    adc_power_off(ADC1);
    if (ADC_CR(ADC1) & ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    if (!_adcs_set_prescale(ADC1, ADCS_CONFIG_PRESCALE))
    {
        log_debug(DEBUG_ADC, "Could not set prescale value.");
    }

    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_enable_vrefint();
    adc_enable_temperature_sensor();
    adc_set_sample_time_on_all_channels(ADC1, ADCS_CONFIG_SAMPLE_TIME);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);

    adc_calibrate(ADC1);

    adc_set_single_conversion_mode(ADC1);
    adc_power_on(ADC1);
}


bool adcs_set_midpoint(uint16_t new_midpoint)
{
    midpoint = new_midpoint;
    return true;
}


void adcs_loop_iteration(void)
{
    if (adcs_running)
    {
        uint8_t local_channel_array[1] = { adc_channel_array[0] };
        adc_set_regular_sequence(ADC1, 1, local_channel_array);
        adc_start_conversion_regular(ADC1);
        while (!(adc_eoc(ADC1)));
        uint16_t new_reading = adc_read_regular(ADC1);
        if (adcs_val.count == 0)
        {
            adcs_val.min = new_reading;
            adcs_val.max = new_reading;
        }
        else if (adcs_val.min > new_reading)
        {
            adcs_val.min = new_reading;
        }
        else if (adcs_val.max < new_reading)
        {
            adcs_val.max = new_reading;
        }
        adcs_val.sum += new_reading;
        adcs_val.count++;
    }
}


bool adcs_begin(char* name)
{
    memset(&adcs_val, 0, sizeof(adc_simple_val_t));
    adcs_running = true;
    return true;
}


bool adcs_calibrate_current_clamp(void)
{
    return true;
}


bool adcs_get_cc(char* name, value_t* value)
{
    adcs_running = false;
    if (adcs_val.count == 0)
    {
        return false;
    }
    uint64_t avg = adcs_val.sum / adcs_val.count;
    *value = value_from_u64(avg);
    return true;
}


bool adcs_get_cc_blocking(char* name, value_t* value)
{
    return adcs_get_cc(name, value);
}


void adcs_init(void)
{
    // Get the midpoint
    if (!persist_get_adc_midpoint(&midpoint))
    {
        // Assume it to be the theoretical midpoint
        log_debug(DEBUG_ADC, "Failed to load persistent midpoint.");
        adcs_set_midpoint(4095 / 2);
    }

    // Setup the clock and gpios
    const port_n_pins_t port_n_pins[] = ADCS_PORT_N_PINS;
    rcc_periph_clock_enable(RCC_ADC1);
    for(unsigned n = 0; n < ARRAY_SIZE(port_n_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(port_n_pins[n].port));
        gpio_mode_setup(port_n_pins[n].port,
                        GPIO_MODE_ANALOG,
                        GPIO_PUPD_NONE,
                        port_n_pins[n].pins);
    }
    // Setup the adc(s)
    _adcs_setup_adc();
}
