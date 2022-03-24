#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "common.h"
#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "uart_rings.h"


#define ADC_CCR_PRESCALE_MASK  (15 << 18)
#define ADC_CCR_PRESCALE_64    ( 9 << 18)


#define ADCS_CONFIG_PRESCALE        ADC_CCR_PRESCALE_64
#define ADCS_CONFIG_SAMPLE_TIME     ADC_SMPR_SMP_640DOT5CYC /* 640.5 + 12.5 cycles of (80Mhz / 64) clock */
/* (640.5 + 12.5) * (1000000 / (80000000 / 64)) = 522.4 microseconds
 *
 * 480 samples
 *
 * 522.4 * 480 = 250752 microseconds
 *
 * So 1/4 a second for all samples.
 *
 */


#define ADCS_CC_DEFAULT_COLLECTION_TIME    1000;

#define ADCS_MON_DEFAULT_COLLECTION_TIME    100;


#define ADC_BAT_MUL   10000UL
#define ADC_BAT_MAX_MV 1343 /* 1.343 volts */
#define ADC_BAT_MIN_MV 791  /* 0.791 volts */
#define ADC_BAT_MAX   (ADC_MAX_VAL * ADC_BAT_MUL / ADC_MAX_MV * ADC_BAT_MAX_MV)
#define ADC_BAT_MIN   (ADC_MAX_VAL * ADC_BAT_MUL / ADC_MAX_MV * ADC_BAT_MIN_MV)


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
    uint8_t     channels[ADC_CC_COUNT];
    unsigned    len;
} adcs_channels_active_t;


typedef uint16_t all_adcs_buf_t[ADCS_NUM_SAMPLES];


static all_adcs_buf_t               adcs_buffer;
static uint8_t                      adc_channel_array[ADC_COUNT]                        = ADC_CHANNELS;
static uint16_t                     cc_midpoints[ADC_CC_COUNT];
static volatile adc_value_status_t  adc_value_status                                    = ADCS_VAL_STATUS_IDLE;

static adcs_channels_active_t       adc_cc_channels_active                              = {0};

static volatile bool                adcs_cc_running                                     = false;
static volatile bool                adcs_bat_running                                    = false;


bool adcs_cc_set_channels_active(uint8_t* active_channels, unsigned len)
{
    if (adcs_cc_running || adcs_bat_running)
    {
        adc_debug("Cannot change phase, ADC reading in progress.");
        return false;
    }
    if (len > ADC_CC_COUNT)
    {
        adc_debug("Not possible length of array.");
        return false;
    }
    memcpy(adc_cc_channels_active.channels, active_channels, len * sizeof(uint8_t));
    adc_cc_channels_active.len = len;
    adc_debug("Setting %"PRIu8" active channels.", len);
    return true;
}


measurements_sensor_state_t adcs_cc_collection_time(char* name, uint32_t* collection_time)
{
    /**
    Could calculate how long it should take to get the results. For now use 2 seconds.
    */
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = ADCS_CC_DEFAULT_COLLECTION_TIME;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
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
        adc_debug("Could not set prescale value.");
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


bool adcs_cc_set_midpoints(uint16_t new_midpoints[ADC_CC_COUNT])
{
    memcpy(cc_midpoints, new_midpoints, ADC_CC_COUNT * sizeof(cc_midpoints[0]));
    if (!persist_set_cc_midpoints(new_midpoints))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


bool adcs_cc_set_midpoint(uint16_t midpoint, uint8_t index)
{
    if (index > ADC_CC_COUNT)
    {
        return false;
    }
    cc_midpoints[index] = midpoint;
    if (!persist_set_cc_midpoints(cc_midpoints))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


/* As the ADC RMS function calculates the RMS of potentially multiple ADCs in a single 
 * buffer, the step and start index are required to find the correct RMS.*/
#ifdef __ADC_RMS_FULL__
static bool _adcs_get_rms(all_adcs_buf_t buff, unsigned buff_len, uint16_t* adc_rms, uint8_t start_index, uint8_t step, uint16_t midpoint)
{
    uint64_t sum = 0;
    int64_t inter_val;
    for (unsigned i = start_index; i < buff_len; i+=step)
    {
        inter_val = buff[i] - midpoint;
        inter_val *= inter_val;
        sum += inter_val;
    }
    *adc_rms = midpoint - 1/Q_rsqrt( sum / (ADCS_NUM_SAMPLES/step) );
    return true;
}
#else
static uint16_t                     peak_vals[ADCS_NUM_SAMPLES];
static bool _adcs_get_rms(all_adcs_buf_t buff, unsigned buff_len, uint16_t* adc_rms, uint8_t start_index, uint8_t step, uint16_t midpoint)
{
    /**
    Four major steps:
    * First find the peaks, this is done with the following criteria:
        - adc val is smaller than previous
        - new wave (and so move position/cursor) when before was below midpoint, now is above midpoint

    * Calculate 'rough' average

    * If peak values are larger than 30% of average then remove.
      This will remove the small noises that may fit the criteria but not be true peaks.

    * This value is divided by sqrt(2) to give the RMS.
    */

    uint32_t sum = 0;
    uint16_t peak_pos = 0;
    peak_vals[0] = buff[start_index];

    // Find the peaks in the data
    for (unsigned i = (start_index + step); i < buff_len; i+=step)
    {
        if (buff[i] < buff[i-1])
        {
            peak_vals[peak_pos] = buff[i];
        }
        else if (buff[i-1] <= midpoint && buff[i] > midpoint)
        {
            sum += peak_vals[peak_pos];
            peak_vals[++peak_pos] = buff[i];
        }
    }

    // Early exit if nothing found
    if (peak_pos == 0)
    {
        adc_debug("Cannot find any peaks.");
        return false;
    }
    uint32_t inter_val = midpoint - sum / peak_pos;
    inter_val /= sqrt(2);
    *adc_rms = midpoint - inter_val;
    return true;
}
#endif //__ADC_RMS_FULL__


static bool _adcs_to_mV(uint16_t value, uint16_t* mV)
{
    // Linear scale without calibration.
    // ADC of    0 -> 0V
    //        4096 -> 3.3V

    const uint16_t max_value = ADC_MAX_VAL;
    const uint16_t min_value = 0;
    const uint16_t max_mV = ADC_MAX_MV;
    const uint16_t min_mV = 0;
    uint32_t inter_val;

    inter_val = value * (max_mV - min_mV);
    inter_val /= (max_value - min_value);

    if (inter_val > UINT16_MAX)
    {
        adc_debug("Cannot downsize value '%"PRIu32"'.", inter_val);
        return false;
    }

    *mV = (uint16_t)inter_val;
    return true;
}


static bool _adcs_current_clamp_conv(uint16_t adc_val, uint16_t* cc_mA, uint16_t midpoint)
{
    /**
     First must calculate the peak voltage
     V_pk = Midpoint - V_adc

     Next convert peak voltage to RMS. For DC this is "=" to it. Kind of.
     For AC it can approximated by V_RMS = 1/sqrt(2) * V_peak

     Now converting from a voltage to a current we use our trusty
     V = I * R      - >     I = V / R
     The resistor used is 22 Ohms.

     The current is scaled by the current clamp to the effect of
     I_t = 2000 * I.

     To retain precision, multiplications should be done first (after casting to
     a uint32_t and then divisions after.
    */
    uint32_t inter_value    = 0;
    uint16_t adc_diff       = 0;

    // If adc_val is larger, then pretend it is at the midpoint
    if (adc_val < midpoint)
    {
        adc_diff = midpoint - adc_val;
    }

    // Once the conversion is no longer linearly multiplicative this needs to be changed.
    if (!_adcs_to_mV(adc_diff, (uint16_t*)&inter_value))
    {
        adc_debug("Cannot get mV value of midpoint.");
        return false;
    }

    if (inter_value > UINT32_MAX / 2000)
    {
        adc_debug("Overflowing value.");
        return false;
    }
    inter_value *= 2000;
    inter_value /= 22;
    if (inter_value > UINT16_MAX)
    {
        adc_debug("Cannot downsize value '%"PRIu32"'.", inter_value);
        return false;
    }
    *cc_mA = (uint16_t)inter_value;
    return true;
}


void tim3_isr(void)
{
    timer_clear_flag(TIM3, TIM_SR_CC1IF);

    static bool     started         = false;
    static uint16_t adcs_buffer_pos = 0;

    if (started)
    {
        if (!adc_eoc(ADC1))
        {
            adc_debug("ADC (%u) not complete!", adcs_buffer_pos);
            return;
        }

        adcs_buffer[adcs_buffer_pos++] = adc_read_regular(ADC1);
    }
    else
    {
        adcs_buffer_pos = 0;
        started = true;
    }

    if (adcs_buffer_pos < ARRAY_SIZE(adcs_buffer))
    {
        adc_set_regular_sequence(ADC1, adc_cc_channels_active.len, adc_cc_channels_active.channels);
        adc_start_conversion_regular(ADC1);
    }
    else
    {
        adcs_cc_running = false;
        started = false;
        timer_disable_counter(TIM3);
    }
}


static bool _adcs_find_active_channel_index(uint8_t* active_channel_index, uint8_t index)
{
    uint8_t adc_channel = adc_channel_array[index];
    for (unsigned i = 0; i < adc_cc_channels_active.len; i++)
    {
        if (adc_cc_channels_active.channels[i] == adc_channel)
        {
            *active_channel_index = i;
            return true;
        }
    }
    return false;
}


static bool _adcs_get_index(uint8_t* index, char* name)
{
    if (strncmp(name, MEASUREMENTS_CURRENT_CLAMP_1_NAME, MEASURE_NAME_LEN) == 0)
    {
        *index = 0;
    }
    else if (strncmp(name, MEASUREMENTS_CURRENT_CLAMP_2_NAME, MEASURE_NAME_LEN) == 0)
    {
        *index = 1;
    }
    else if (strncmp(name, MEASUREMENTS_CURRENT_CLAMP_3_NAME, MEASURE_NAME_LEN) == 0)
    {
        *index = 2;
    }
    else
    {
        adc_debug("'%s' is not a current clamp name.", name);
        return false;
    }
    return true;
}


measurements_sensor_state_t adcs_cc_begin(char* name)
{
    if (adcs_bat_running)
    {
        adc_debug("ADCs already running.");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }
    if (adcs_cc_running)
    {
        return MEASUREMENTS_SENSOR_STATE_SUCCESS;
    }
    adc_debug("Started ADC reading for CC.");
    adcs_cc_running = true;
    timer_set_counter(TIM3, 0);
    timer_enable_counter(TIM3);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static void _adcs_cc_wait(void)
{
    adc_debug("Waiting for ADC CC");
    while (adcs_cc_running)
        uart_rings_out_drain();
}


bool adcs_cc_calibrate(void)
{
    uint8_t all_cc_channels[ADC_CC_COUNT] = ADC_CC_CHANNELS;
    adcs_channels_active_t prev_adc_cc_channels_active = {0};
    memcpy(prev_adc_cc_channels_active.channels, adc_cc_channels_active.channels, adc_cc_channels_active.len * sizeof(adc_cc_channels_active.channels[0]));
    prev_adc_cc_channels_active.len = adc_cc_channels_active.len;

    memcpy(adc_cc_channels_active.channels, all_cc_channels, ADC_CC_COUNT);
    adc_cc_channels_active.len = ADC_CC_COUNT;

    if (adcs_cc_begin("") != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        return false;
    }
    _adcs_cc_wait();
    uint64_t sum;
    uint16_t midpoints[ADC_CC_COUNT];
    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
    {
        sum = 0;
        for (unsigned j = 0; j < ADCS_NUM_SAMPLES; j+=ADC_CC_COUNT)
        {
            sum += adcs_buffer[i+j];
        }
        midpoints[i] = sum / (ADCS_NUM_SAMPLES / ADC_CC_COUNT);
        adc_debug("Midpoint[%u] = %"PRIu16, i, midpoints[i]);
    }
    adcs_cc_set_midpoints(midpoints);
    memcpy(adc_cc_channels_active.channels, prev_adc_cc_channels_active.channels, prev_adc_cc_channels_active.len);
    adc_cc_channels_active.len = prev_adc_cc_channels_active.len;
    return true;
}


measurements_sensor_state_t adcs_cc_get(char* name, value_t* value)
{
    if (adcs_cc_running)
    {
        adc_debug("ADCs not finished.");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }

    uint16_t adcs_rms = 0;

    uint8_t index, active_index;

    if (!_adcs_get_index(&index, name))
    {
        adc_debug("Cannot get index.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!_adcs_find_active_channel_index(&active_index, index))
    {
        adc_debug("Not in active channel.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint16_t midpoint = cc_midpoints[index];

    if (!_adcs_get_rms(adcs_buffer, ARRAY_SIZE(adcs_buffer), &adcs_rms, active_index, adc_cc_channels_active.len, midpoint))
    {
        adc_debug("Failed to get RMS");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint16_t cc_mA = 0;

    if (!_adcs_current_clamp_conv(adcs_rms, &cc_mA, midpoint))
    {
        adc_debug("Failed to get current clamp");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    *value = value_from_u16(cc_mA);
    adc_debug("CC = %umA", cc_mA);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool adcs_cc_get_blocking(char* name, value_t* value)
{
    if (!adcs_cc_begin(name))
        return false;
    _adcs_cc_wait();
    return adcs_cc_get(name, value);
}


measurements_sensor_state_t adcs_bat_begin(char* name)
{
    if (adcs_cc_running || adcs_bat_running)
    {
        adc_debug("ADCs already running.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    adc_debug("Started ADC reading for BAT.");
    adcs_bat_running = true;
    adc_set_regular_sequence(ADC1, 1, &adc_channel_array[ADC_INDEX_BAT_MON]);
    adc_start_conversion_regular(ADC1);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t adcs_bat_get(char* name, value_t* value)
{
    if (!adcs_bat_running)
    {
        adc_debug("ADC for Bat not running!");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    adcs_bat_running = false;

    if (!adc_eoc(ADC1))
    {
        adc_debug("ADC for Bat not complete!");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }

    if (!value)
    {
        adc_debug("Handed NULL Pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    unsigned raw = adc_read_regular(ADC1);

    adc_debug("Bat raw ADC:%u", raw);

    raw *= ADC_BAT_MUL;

    uint16_t perc;

    if (raw > ADC_BAT_MAX)
    {
        perc = ADC_BAT_MUL;
    }
    else if (raw < ADC_BAT_MIN)
    {
        /* How are we here? */
        perc = (uint16_t)ADC_BAT_MIN;
    }
    else
    {
        uint32_t divider =  (ADC_BAT_MAX / ADC_BAT_MUL) - (ADC_BAT_MIN / ADC_BAT_MUL);
        perc = (raw - ADC_BAT_MIN) / divider;
    }

    *value = value_from(perc);

    adc_debug("Bat %u.%02u", perc / 100, perc %100);

    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t adcs_bat_collection_time(char* name, uint32_t* collection_time)
{
    /**
    Could calculate how long it should take to get the results. For now use 2 seconds.
    */
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = ADCS_MON_DEFAULT_COLLECTION_TIME;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool adcs_bat_get_blocking(char* name, value_t* value)
{
    if (!adcs_bat_begin(name))
        return false;
    adc_debug("Waiting for ADC BAT");
    while (!adc_eoc(ADC1))
        uart_rings_out_drain();
    return adcs_bat_get(name, value);
}


void adcs_init(void)
{
    // Get the midpoint
    if (!persist_get_cc_midpoints(cc_midpoints))
    {
        // Assume it to be the theoretical midpoint
        adc_debug("Failed to load persistent midpoint.");
        uint16_t midpoints[ADC_CC_COUNT] = {ADC_MAX_VAL / 2};
        adcs_cc_set_midpoints(midpoints);
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

    rcc_periph_clock_enable(RCC_TIM3);

    timer_disable_counter(TIM3);

    timer_set_mode(TIM3,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);

    timer_set_prescaler(TIM3, rcc_ahb_frequency / 10000-1);//-1 because it starts at zero, and interrupts on the overflow
    timer_set_period(TIM3, 5);
    timer_enable_preload(TIM3);
    timer_continuous_mode(TIM3);
    timer_enable_irq(TIM3, TIM_DIER_CC1IE);

    nvic_enable_irq(NVIC_TIM3_IRQ);
}
