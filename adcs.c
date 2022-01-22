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


typedef uint16_t all_adcs_buf_t[ADC_DMA_CHANNELS_COUNT][ADCS_NUM_SAMPLES*ADC_COUNT];


static uint16_t                     adcs_buffer_pos                                     = 0;
static all_adcs_buf_t               adcs_buffer;
static bool                         adcs_running                                        = false;
static uint8_t                      adc_channel_array[ADC_COUNT]                        = ADC_CHANNELS;
static uint16_t                     midpoint;
static volatile adc_value_status_t  adc_value_status                                    = ADCS_VAL_STATUS_IDLE;
static uint16_t                     peak_vals[ADCS_NUM_SAMPLES];


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


bool adcs_set_midpoint(uint16_t new_midpoint)
{
    midpoint = new_midpoint;
    if (!persist_set_adc_midpoint(new_midpoint))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


#ifdef __ADC_RMS_FULL__
static float _Q_rsqrt( float number )
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
    y  = * ( float * ) &i;
    #pragma GCC diagnostic pop
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//  y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}

static bool _adcs_get_rms(all_adcs_buf_t buff, unsigned buff_len, uint16_t* adc_rms)
{
    uint64_t sum = 0;
    for (unsigned i = 0; i < buff_len; i++)
    {
        sum += buff[0][i] * buff[0][i];
    }
    *adc_rms = midpoint - 1/_Q_rsqrt( ( sum / ADCS_NUM_SAMPLES ) - midpoint );
    return true;
}
#else
static bool _adcs_get_rms(all_adcs_buf_t buff, unsigned buff_len, uint16_t* adc_rms)
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
    peak_vals[0] = buff[0][0];

    // Find the peaks in the data
    for (unsigned i = 1; i < buff_len; i++)
    {
        if (buff[0][i] < buff[0][i-1])
        {
            peak_vals[peak_pos] = buff[0][i];
        }
        else if (buff[0][i-1] <= midpoint && buff[0][i] > midpoint)
        {
            sum += peak_vals[peak_pos];
            peak_vals[++peak_pos] = buff[0][i];
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

    uint16_t max_value = 4095;
    uint16_t min_value = 0;
    uint16_t max_mV = 3300;
    uint16_t min_mV = 0;
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


static bool _adcs_current_clamp_conv(uint16_t adc_val, uint16_t* cc_mA)
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


void adcs_loop_iteration(void)
{
    if (adcs_running)
    {
        uint8_t local_channel_array[1] = { adc_channel_array[0] };
        adc_set_regular_sequence(ADC1, 1, local_channel_array);
        adc_start_conversion_regular(ADC1);
        while (!(adc_eoc(ADC1)));

        adcs_buffer[0][adcs_buffer_pos++] = adc_read_regular(ADC1);
        if (adcs_buffer_pos == sizeof(adcs_buffer[0]) / sizeof(uint16_t))
        {
            adcs_running = false;
        }
    }
}


bool adcs_begin(char* name)
{
    adcs_buffer_pos = 0;
    adcs_running = true;
    return true;
}


bool adcs_calibrate_current_clamp(void)
{
    adcs_begin("");
    while (adcs_running)
    {
        adcs_loop_iteration();
    }
    uint64_t sum = 0;
    for (unsigned i = 0; i < adcs_buffer_pos; i++)
    {
        sum += adcs_buffer[0][i];
    }
    adcs_set_midpoint(sum / adcs_buffer_pos);
    return true;
}


bool adcs_get_cc(char* name, value_t* value)
{
    adcs_running = false;

    uint16_t adcs_rms = 0;
    uint16_t cc_mA = 0;
    if (!_adcs_get_rms(adcs_buffer, adcs_buffer_pos, &adcs_rms))
    {
        return false;
    }
    if (!_adcs_current_clamp_conv(adcs_rms, &cc_mA))
    {
        return false;
    }
    *value = value_from_u16(cc_mA);
    return true;
}


bool adcs_get_cc_blocking(char* name, value_t* value)
{
    adcs_begin(name);
    while (adcs_running)
    {
        adcs_loop_iteration();
    }
    return adcs_get_cc(name, value);
}


void adcs_init(void)
{
    // Get the midpoint
    if (!persist_get_adc_midpoint(&midpoint))
    {
        // Assume it to be the theoretical midpoint
        adc_debug("Failed to load persistent midpoint.");
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
