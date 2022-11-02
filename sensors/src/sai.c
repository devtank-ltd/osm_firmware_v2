/**
A driver for a ICS 43434 microphone using the STM32 SAI by Devtank Ltd.

Documents used:
- STM32 SAI for STM32L4XXX Reference Manual RM0394
    : https://usermanual.wiki/Document/STM32L420Reference20manual.1774855139/view
      (Accessed: 07.09.21
- ICS-43434 Microphone Datasheet
    : https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf
      (Accessed: 21.02.22)
*/

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rcc.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "pinmap.h"
#include "log.h"
#include "uart_rings.h"
#include "measurements.h"
#include "persist_config.h"

#define SAI1   SAI1_BASE


/** Configuration register 1 (SAI_xCR1) */
#define SAI_xCR1(sai, block)    MMIO32(sai + 0x04 + 0x20 * block)
#define SAI1_ACR1   SAI_xCR1(SAI1, 0)
#define SAI1_BCR1   SAI_xCR1(SAI1, 1)

/** Configuration register 2 (SAI_xCR2)*/
#define SAI_xCR2(sai, block)    MMIO32(sai + 0x08 + 0x20 * block)
#define SAI1_ACR2   SAI_xCR2(SAI1, 0)
#define SAI1_BCR2   SAI_xCR2(SAI1, 1)

/** Frame configuration register (SAI_FRCR)*/
#define SAI_xFRCR(sai, block)   MMIO32(sai + 0x0C + 0x20 * block)
#define SAI1_AFRCR  SAI_xFRCR(SAI1, 0)
#define SAI1_BFRCR  SAI_xFRCR(SAI1, 1)

/** Slot register (SAI_SLOTR)*/
#define SAI_xSLOTR(sai, block)  MMIO32(sai + 0x10 + 0x20 * block)
#define SAI1_ASLOTR SAI_xSLOTR(SAI1, 0)
#define SAI1_BSLOTR SAI_xSLOTR(SAI1, 1)

/** Interrupt mask register (SAI_IM)*/
#define SAI_xIM(sai, block)     MMIO32(sai + 0x14 + 0x20 * block)
#define SAI1_AIM    SAI_xIM(SAI1, 0)
#define SAI1_BIM    SAI_xIM(SAI1, 1)

/** Status register (SAI_SR)*/
#define SAI_xSR(sai, block)     MMIO32(sai + 0x18 + 0x20 * block)
#define SAI1_ASR    SAI_xSR(SAI1, 0)
#define SAI1_BSR    SAI_xSR(SAI1, 1)

/** Clear flag register (SAI_CLRFR)*/
#define SAI_xCLRFR(sai, block)  MMIO32(sai + 0x1C + 0x20 * block)
#define SAI1_ACLRFR SAI_xCLRFR(SAI1, 0)
#define SAI1_BCLRFR SAI_xCLRFR(SAI1, 1)

/** Data register (SAI_DR)*/
#define SAI_xDR(sai, block)     MMIO32(sai + 0x20 + 0x20 * block)
#define SAI1_ADR    SAI_xDR(SAI1, 0)
#define SAI1_BDR    SAI_xDR(SAI1, 1)


/* RCC_PLLSAI1_CFGR Values -------------------------------------------*/
#define RCC_PLLSAIx_CFGR_PLLSAI1PDIV_MASK  0x1F
#define RCC_PLLSAIx_CFGR_PLLSAI1PDIV_SHIFT 27
#define RCC_PLLSAIx_CFGR_PLLSAI1R_MASK     0x3
#define RCC_PLLSAIx_CFGR_PLLSAI1R_SHIFT    25
#define RCC_PLLSAIx_CFGR_PLLSAI1REN        (1 << 24)
#define RCC_PLLSAIx_CFGR_PLLSAI1Q_MASK     0x3
#define RCC_PLLSAIx_CFGR_PLLSAI1Q_SHIFT    21
#define RCC_PLLSAIx_CFGR_PLLSAI1QEN        (1 << 20)
#define RCC_PLLSAIx_CFGR_PLLSAI1P          (1 << 17)
#define RCC_PLLSAIx_CFGR_PLLSAI1PEN        (1 << 16)
#define RCC_PLLSAIx_CFGR_PLLSAI1N_MASK     0x7F
#define RCC_PLLSAIx_CFGR_PLLSAI1N_SHIFT    8




/* SAI_xCR1 Values ---------------------------------------------------*/
#define SAI_xCR1_MCKDIV_SHIFT   20
#define SAI_xCR1_MCKDIV_MASK    0xF
#define SAI_xCR1_NODIV          (1 << 19)
#define SAI_xCR1_DMAEN          (1 << 17)
#define SAI_xCR1_SAIEN          (1 << 16)
#define SAI_xCR1_OUTDRIV        (1 << 13)
#define SAI_xCR1_MONO           (1 << 12)
#define SAI_xCR1_SYNCEN_SHIFT   10
#define SAI_xCR1_SYNCEN_MASK    0x3
#define SAI_xCR1_CKSTR          (1 << 9)
#define SAI_xCR1_LSBFIRST       (1 << 8)
#define SAI_xCR1_DS_SHIFT       5
#define SAI_xCR1_DS_MASK        0x7
#define SAI_xCR1_PRTCFG_SHIFT   3
#define SAI_xCR1_PRTCFG_MASK    0x3
#define SAI_xCR1_MODE_SHIFT     0
#define SAI_xCR1_MODE_MASK      0x3


enum sai_xcr1_syncen {
    SAI_xCR1_SYNCEN_ASYNC = 0,
    SAI_xCR1_SYNCEN_SYNC  = 1,
};

enum sai_xcr1_ds {
    SAI_xCR1_DS_8BIT  = 2,
    SAI_xCR1_DS_10BIT = 3,
    SAI_xCR1_DS_16BIT = 4,
    SAI_xCR1_DS_20BIT = 5,
    SAI_xCR1_DS_24BIT = 6,
    SAI_xCR1_DS_32BIT = 7,
};

enum sai_xcr1_prtcfg {
    SAI_xCR1_PRTCFG_FREE  = 0,
    SAI_xCR1_PRTCFG_SPDIF = 1,
    SAI_xCR1_PRTCFG_AC97  = 2,
};

enum sai_xcr1_mode {
    SAI_xCR1_MODE_MASTER_TRANS = 0,
    SAI_xCR1_MODE_MASTER_RECV = 1,
    SAI_xCR1_MODE_SLAVE_TRANS = 2,
    SAI_xCR1_MODE_SLAVE_RECV = 3,
};


/* SAI_xCR2 Values ---------------------------------------------------*/
#define SAI_xCR2_COMP_MASK     0x3
#define SAI_xCR2_COMP_SHIFT    14
#define SAI_xCR2_CPL           (1 << 13)
#define SAI_xCR2_MUTECNT_MASK  0x3F
#define SAI_xCR2_MUTECNT_SHIFT 7
#define SAI_xCR2_MUTEVAL       (1 << 6)
#define SAI_xCR2_MUTE          (1 << 5)
#define SAI_xCR2_TRIS          (1 << 4)
#define SAI_xCR2_FFLUSH        (1 << 3)
#define SAI_xCR2_FTH_MASK      (0x3)
#define SAI_xCR2_FTH_SHIFT     0

enum sai_xcr2_fth {
    SAI_xCR2_FTH_FIFO_EMPTY = 0,
    SAI_xCR2_FTH_FIFO_1_4   = 1,
    SAI_xCR2_FTH_FIFO_1_2   = 2,
    SAI_xCR2_FTH_FIFO_3_4   = 3,
    SAI_xCR2_FTH_FIFO_FULL  = 4,
};

#define SAI_xCR2_COMP_NONE        0
#define SAI_xCR2_COMP_A_LAW       0
#define SAI_xCR2_COMP_MU_LAW      0


/* SAI_xFRCR Values --------------------------------------------------*/

#define SAI_xFRCR_FSOFF       (1 << 18)
#define SAI_xFRCR_FSPOL       (1 << 17)
#define SAI_xFRCR_FSDEF       (1 << 16)
#define SAI_xFRCR_FSALL_MASK  0x3F
#define SAI_xFRCR_FSALL_SHIFT 8
#define SAI_xFRCR_FRL_MASK    0xFF
#define SAI_xFRCR_FRL_SHIFT   0


/* SAI_xSLOTR Values -------------------------------------------------*/

#define SAI_xSLOTR_SLOTEN_MASK    0xFFFF
#define SAI_xSLOTR_SLOTEN_SHIFT   16
#define SAI_xSLOTR_NBSLOT_MASK    0xF
#define SAI_xSLOTR_NBSLOT_SHIFT   8
#define SAI_xSLOTR_SLOTSZ_MASK    0x3
#define SAI_xSLOTR_SLOTSZ_SHIFT   6
#define SAI_xSLOTR_FBOFF_MASK     0x1F
#define SAI_xSLOTR_FBOFF_SHIFT    0

#define SAI_xSLOTR_SLOTEN_ALL     SAI_xSLOTR_SLOTEN_MASK

enum sai_xslotr_slotsz {
    SAI_xSLOTR_SLOTSZ_SLOTSIZE = 0,
    SAI_xSLOTR_SLOTSZ_16BIT    = 1,
    SAI_xSLOTR_SLOTSZ_32BIT    = 2,
};


/* SAI_xIM Values ----------------------------------------------------*/

#define SAI_xIM_LFSDETIE    (1 << 6)
#define SAI_xIM_AFSDETIE    (1 << 5)
#define SAI_xIM_CNRDYIE     (1 << 4)
#define SAI_xIM_FREQIE      (1 << 3)
#define SAI_xIM_WCKCFGIE    (1 << 2)
#define SAI_xIM_MUTEDETIE   (1 << 1)
#define SAI_xIM_OVRUDRIE    (1 << 0)

/* SAI_xSR Values ----------------------------------------------------*/

#define SAI_xSR_FLVL_MASK  0x7
#define SAI_xSR_FLVL_SHIFT 16
#define SAI_xSR_LFSDET     (1 << 6)
#define SAI_xSR_AFSDET     (1 << 5)
#define SAI_xSR_CNRDY      (1 << 4)
#define SAI_xSR_FREQ       (1 << 3)
#define SAI_xSR_WCKCFG     (1 << 2)
#define SAI_xSR_MUTEDET    (1 << 1)
#define SAI_xSR_OVRUDR     (1 << 0)

enum sai_xsr_flvl {
    SAI_xSR_FLVL_EMPTY          = 0,
    SAI_xSR_FLVL_LTE_1_4        = 1,
    SAI_xSR_FLVL_GTE_1_4_LT_1_2 = 2,
    SAI_xSR_FLVL_GTE_1_2_LT_3_4 = 3,
    SAI_xSR_FLVL_GTE_3_4        = 4,
    SAI_xSR_FLVL_FULL           = 5,
};

/* SAI_CLRFR Values --------------------------------------------------*/

#define SAI_xCLRFR_CLFSDET  (1 << 6)
#define SAI_xCLRFR_CAFSDET  (1 << 5)
#define SAI_xCLRFR_CCNRDY   (1 << 4)
#define SAI_xCLRFR_CWCKCFG  (1 << 2)
#define SAI_xCLRFR_CMUTEDET (1 << 1)
#define SAI_xCLRFR_COVRUDR  (1 << 0)

#define SAI_NB_SLOTS                        2
#define SAI_FRL                             64

#define SAI_DEFAULT_COLLECTION_TIME         1000
#define SAI_INIT_DELAY                      25

#define SAI_NUM_SAMPLES                     10
#define SAI_ARRAY_SIZE                      (SAI_NUM_SAMPLES * SAI_NB_SLOTS)
#define SAI_NUM_OFLOW_SCALES                5

#define SAI_DEFAULT_COEFFS     {                                       \
    -1.4476694634028f ,                                                \
    +32.842913823748f ,                                                \
    -277.21914042017f ,                                                \
    +1051.3790253415f ,                                                \
    -1443.2063094441f                                                  \
    }


typedef volatile    int32_t     sai_arr_t[SAI_ARRAY_SIZE];
typedef const       uint32_t    sai_overflow_arr[SAI_NUM_OFLOW_SCALES];


typedef struct
{
    uint64_t    rolling_rms;
    uint32_t    num_rms;
    bool        finished;
} sai_sample_t;


static sai_arr_t             _sai_array              = {0};
static sai_overflow_arr      _sai_overflow_scales    = { 1, 10, 100, 1000, 10000 };
static float               * _sai_calibration_coeffs = NULL;

static volatile sai_sample_t _sai_sample          = {.rolling_rms=0,
                                                     .num_rms=0,
                                                     .finished=false};


static void _sai_clock_off(void)
{
    RCC_CR &= ~RCC_CR_PLLSAI1ON;
    while(RCC_CR & RCC_CR_PLLSAI1RDY);
}


static void _sai_clock_on(void)
{
    RCC_CR |= RCC_CR_PLLSAI1ON;
    while(!(RCC_CR & RCC_CR_PLLSAI1RDY));
}


static void _sai_dma_off(void)
{
    dma_disable_channel(DMA2, DMA_CHANNEL1);
}


static void _sai_dma_on(void)
{
    dma_enable_channel(DMA2, DMA_CHANNEL1);
}


static void _sai_dma_init(void)
{
    dma_channel_reset(DMA2, DMA_CHANNEL1);

    dma_set_peripheral_address(DMA2, DMA_CHANNEL1, (uint32_t)&SAI1_ADR);
    dma_set_memory_address(DMA2, DMA_CHANNEL1, (uint32_t)_sai_array);
    dma_set_number_of_data(DMA2, DMA_CHANNEL1, ARRAY_SIZE(_sai_array));
    dma_set_read_from_peripheral(DMA2, DMA_CHANNEL1);
    dma_enable_memory_increment_mode(DMA2, DMA_CHANNEL1);
    dma_set_peripheral_size(DMA2, DMA_CHANNEL1, DMA_CCR_PSIZE_32BIT);
    dma_set_memory_size(DMA2, DMA_CHANNEL1, DMA_CCR_MSIZE_32BIT);
    dma_set_priority(DMA2, DMA_CHANNEL1, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(DMA2, DMA_CHANNEL1);
}


static bool _sai_load_coeffs(void)
{
    _sai_calibration_coeffs = persist_get_sai_cal_coeffs();
    for (unsigned i = 0; i < SAI_NUM_CAL_COEFFS; i++)
    {
        if (_sai_calibration_coeffs[i] != 0)
            return true;
    }
    return false; /* If the coeffs are all zero, it's not valid/set. */
}


void sai_init(void)
{
    const port_n_pins_t sai_pins[]  = SAI_PORT_N_PINS;
    const uint32_t      sai_pin_funcs[] = SAI_PORT_N_PINS_AF;

    rcc_periph_clock_enable(SCC_SAI1);

    RCC_PLLSAI1_CFGR &= (RCC_PLLSAIx_CFGR_PLLSAI1PEN | RCC_PLLSAIx_CFGR_PLLSAI1QEN);

    _sai_clock_off();

    SAI1_ACR1   = ((SAI_xCR1_MODE_MASTER_RECV   & SAI_xCR1_MODE_MASK        ) << SAI_xCR1_MODE_SHIFT     )|
                  ((SAI_xCR1_PRTCFG_FREE        & SAI_xCR1_PRTCFG_MASK      ) << SAI_xCR1_PRTCFG_SHIFT   )|
                  ((SAI_xCR1_DS_24BIT           & SAI_xCR1_DS_MASK          ) << SAI_xCR1_DS_SHIFT       )|
                  ((2                           & SAI_xCR1_MCKDIV_MASK      ) << SAI_xCR1_MCKDIV_SHIFT   )|
                  ( SAI_xCR1_MONO  | SAI_xCR1_CKSTR | SAI_xCR1_SAIEN                                     );


    SAI1_AFRCR  = (( (   SAI_FRL-1)             & SAI_xFRCR_FRL_MASK        ) << SAI_xFRCR_FRL_SHIFT     )|
                  ((((SAI_FRL/2)-1)             & SAI_xFRCR_FSALL_MASK      ) << SAI_xFRCR_FSALL_SHIFT   )|
                  ( SAI_xFRCR_FSOFF                                                                      );

    SAI1_ASLOTR = (( (   SAI_NB_SLOTS  -1)      & SAI_xSLOTR_NBSLOT_MASK    ) << SAI_xSLOTR_NBSLOT_SHIFT )|
                  (( SAI_xSLOTR_SLOTEN_ALL      & SAI_xSLOTR_SLOTEN_MASK    ) << SAI_xSLOTR_SLOTEN_SHIFT )|
                  (( SAI_xSLOTR_SLOTSZ_32BIT    & SAI_xSLOTR_SLOTSZ_MASK    ) << SAI_xSLOTR_SLOTSZ_SHIFT );

/*
• f(VCOSAI1 clock) = f(PLL clock input) × (PLLSAI1N / PLLM)    (VCOSAI1 clock) = 16Mhz * (16 / 4) = 64Mhz
• f(PLLSAI1_P) = f(VCOSAI1 clock) / PLLSAI1P                   PLLSAI1_P = 64Mhz / 8 = 8Mhz
• f(PLLSAI1_Q) = f(VCOSAI1 clock) / PLLSAI1Q
• f(PLLSAI1_R) = f(VCOSAI1 clock) / PLLSAI1R */

    RCC_PLLSAI1_CFGR = (8 << RCC_PLLSAIx_CFGR_PLLSAI1PDIV_SHIFT) |
                       (16 << RCC_PLLSAIx_CFGR_PLLSAI1N_SHIFT) |
                       RCC_PLLSAIx_CFGR_PLLSAI1PEN;

    _sai_clock_on();

    for(unsigned n = 0; n < ARRAY_SIZE(sai_pins); n++)
    {
        port_n_pins_t sai_pin = sai_pins[n];

        rcc_periph_clock_enable(PORT_TO_RCC(sai_pin.port));
        gpio_mode_setup(sai_pins[n].port,
                        GPIO_MODE_AF,
                        GPIO_PUPD_NONE,
                        sai_pins[n].pins);

        gpio_set_af( sai_pin.port, sai_pin_funcs[n], sai_pin.pins );
    }

    rcc_periph_clock_enable(RCC_DMA2);
    nvic_enable_irq(NVIC_DMA2_CHANNEL1_IRQ);
    dma_set_channel_request(DMA2, DMA_CHANNEL1, 1); /*SAI1_A*/

    if (!_sai_load_coeffs())
    {
        const float _sai_default_calibration_coeffs[SAI_NUM_CAL_COEFFS] = SAI_DEFAULT_COEFFS;
        sound_debug("No calibration values found, using default.");
        memcpy(_sai_calibration_coeffs, _sai_default_calibration_coeffs, sizeof(_sai_default_calibration_coeffs));
    }

    _sai_dma_init();

    SAI1_ACR1 |= SAI_xCR1_DMAEN;
}


static bool _sai_rms(uint32_t* rms, const sai_arr_t arr, unsigned len, uint32_t downscale)
{
    /*
     This is a bad way of calculating RMS. Need to look at convoltion
     to minimise cycles and memory usage.
     */
    uint64_t sum = 0;
    for (unsigned i = 0; i < len; i++)
    {
        // convert to signed 24 bit number
        int32_t val = arr[i];
        if (val & 0xC00000)
            val |= 0xFF000000;

        // scale down to avoid overflowing
        val /= downscale;

        // See if the square is larger than max uint64
        uint64_t val_sqr = abs_i64(val);
        if (val_sqr >= UINT64_MAX / val_sqr)
        {
            sound_debug("Overflow with scale %"PRIu32, downscale);
            return false;
        }
        val_sqr *= val_sqr;

        // See if the sum is larger than max uint64
        if (sum > UINT64_MAX - val_sqr)
        {
            sound_debug("Overflow with scale %"PRIu32, downscale);
            return false;
        }
        sum = sum + val_sqr;
    }
    // Difference due to fast sqrt is +-0.1%. This is
    // +-(0.1 * 1/ln(10))% = 0.04% after logging the RMS.
    *rms = 1/Q_rsqrt(sum/len) * downscale;
    return true;
}


static bool _sai_rms_adaptive(uint32_t* rms, sai_arr_t arr, unsigned len)
{
    /*
     This function uses a scale to downscale the amplitudes before
     calculating the RMS to ensure the value doesn't overflow.
     */
    uint32_t rms_local;
    uint32_t overflow_scale_index = 0;
    while (!_sai_rms(&rms_local, arr, len, _sai_overflow_scales[overflow_scale_index]))
    {
        if (overflow_scale_index >= SAI_NUM_OFLOW_SCALES)
        {
            sound_debug("Cannot downscale any more.");
            return false;
        }
        overflow_scale_index++;
    }
    *rms = rms_local;
    return true;
}


static uint32_t _sai_conv_dB(uint64_t rms)
{
    /*
     The real unit calculated here is centibel as everything is
     multiplied by 10 for higher precision in the reading.
     Conversion formula:
        dB = 20 * log₁₀ (amp_rms/amp_ref)
     Cannot afford to do an integer log₁₀ as fidelity of the precision
     is too low.

     The calibration was found by getting the RMS of the amplitudes
     for dB between the range of 35.1 and 122.5. The log₁₀ (RMS) is
     plotted against the dB on the soundmeter, a polynomial fit of order
     4 was used to generate the following values.
        f(x) = A·x⁴ + B·x³ + C·x² + F·x + E
     Where: A = -1.4476694634028
            B = +32.842913823748
            C = -277.21914042017
            D = +1051.3790253415
            E = -1443.2063094441
     */
    float x = log10(rms);
    uint32_t dB = 10 * (  _sai_calibration_coeffs[0] * x * x * x * x
                        + _sai_calibration_coeffs[1] * x * x * x
                        + _sai_calibration_coeffs[2] * x * x
                        + _sai_calibration_coeffs[3] * x
                        + _sai_calibration_coeffs[4]);
    /* As the minimum value for calibration was 35dB any numbers below
       will be ignored, setting a hard minimum of 35dB. Expected values
       will be above this anyway.
       The maximum value calibrated with was 122.5dB, so if a value is
       over, unknown behaviour, limiting to 122.5dB. Expected values
       should be below this anyway.
    */
    if (dB < 351)
        dB = 351;
    if (dB > 1225)
        dB = 1225;
    return dB;
}



static bool _sai_collect(void)
{
    /*
     * Calculate the rolling RMS with an adaptive window size of the
     * number of samples.
     * Total RMS =    _________________________________
     *               /old_rms² x num_old_rms + new_rms²
     *              / ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     *             V         num_old_rms + 1
     */
    uint64_t prev_rms_cmp, new_rms_cmp, rms;
    uint32_t rms_32;

    _sai_sample.num_rms++;

    if (!u64_multiply_overflow_check(&prev_rms_cmp, _sai_sample.rolling_rms, _sai_sample.rolling_rms))
    {
        goto overflow_exit;
    }

    if (!u64_multiply_overflow_check(&prev_rms_cmp, prev_rms_cmp, (_sai_sample.num_rms-1)))
    {
        goto overflow_exit;
    }

    if (!_sai_rms_adaptive(&rms_32, _sai_array, SAI_ARRAY_SIZE))
    {
        sound_debug("Cannot collect RMS.");
        return false;
    }
    new_rms_cmp = rms_32;

    if (!u64_multiply_overflow_check(&new_rms_cmp, new_rms_cmp, new_rms_cmp))
    {
        goto overflow_exit;
    }

    if (!u64_addition_overflow_check(&rms, prev_rms_cmp, new_rms_cmp))
    {
        goto overflow_exit;
    }

    rms /= _sai_sample.num_rms;
    /* Difference due to fast sqrt is +-0.1%. This is
     * +-(0.1 * 1/ln(10))% = 0.04% after logging the RMS.
     * Companding of the errors are small enough to ignore.
     */
    rms = 1/Q_rsqrt(rms);
    _sai_sample.rolling_rms = rms;
    return true;
overflow_exit:
    sound_debug("Overflow issue.");
    return false;
}


// cppcheck-suppress unusedFunction ; System handler
void dma2_channel1_isr(void)
{
    DMA2_IFCR |= DMA_IFCR_CTCIF(DMA_CHANNEL1);
    _sai_sample.finished = true;
}


static measurements_sensor_state_t _sai_iteration_callback(char* name)
{
    if (_sai_sample.finished)
    {
        _sai_sample.finished = false;
        if (_sai_collect())
        {
            _sai_dma_init();
            _sai_dma_on();
        }
        else sound_debug("Failed to collect.");
    }
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _sai_collection_time(char* name, uint32_t* collection_time)
{
    *collection_time = SAI_DEFAULT_COLLECTION_TIME;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _sai_measurements_init(char* name, bool in_isolation)
{
    _sai_dma_init();
    _sai_dma_on();
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _sai_measurements_get(char* name, measurements_reading_t* value)
{
    _sai_dma_off();
    if (_sai_sample.num_rms == 0)
    {
        sound_debug("No samples computed.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    uint32_t num_samples = _sai_sample.num_rms * SAI_ARRAY_SIZE;
    uint32_t dB = _sai_conv_dB(_sai_sample.rolling_rms);

    // Reset the rolling RMS
    _sai_sample.num_rms = 0;

    sound_debug("Total RMS = %"PRIu64, _sai_sample.rolling_rms);
    sound_debug("%"PRIu32".%"PRIu32" dB from %"PRIu32" samples.", dB/10, dB%10, num_samples);
    value->v_i64 = (int64_t)dB;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void sai_print_coeffs(void)
{
    for (unsigned i = 0; i < SAI_NUM_CAL_COEFFS; i++)
    {
        log_out("Coeff[%u] = %f", i+1, _sai_calibration_coeffs[i]);
    }
}


bool sai_set_coeff(uint8_t index, float coeff)
{
    if (index > SAI_NUM_CAL_COEFFS)
        return false;
    _sai_calibration_coeffs[index] = coeff;
    return true;
}


void  sai_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _sai_collection_time;
    inf->init_cb            = _sai_measurements_init;
    inf->get_cb             = _sai_measurements_get;
    inf->iteration_cb       = _sai_iteration_callback;
    inf->value_type         = MEASUREMENTS_VALUE_TYPE_I64;
}
