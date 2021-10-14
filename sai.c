
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <inttypes.h>
#include "config.h"
#include "pinmap.h"
#include "log.h"
#include "uart_rings.h"

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






#define USED_SLOTS 8


volatile bool audio_dumping = false;
volatile bool start_audio_dumping = false;

static uint32_t sai_slots[USED_SLOTS/2] = {0};


static void _sai_dma_init(void)
{
    dma_channel_reset(DMA2, DMA_CHANNEL1);

    dma_set_peripheral_address(DMA2, DMA_CHANNEL1, (uint32_t)&SAI1_ADR);
    dma_set_memory_address(DMA2, DMA_CHANNEL1, (uint32_t)sai_slots);
    dma_set_number_of_data(DMA2, DMA_CHANNEL1, ARRAY_SIZE(sai_slots));
    dma_set_read_from_peripheral(DMA2, DMA_CHANNEL1);
    dma_enable_memory_increment_mode(DMA2, DMA_CHANNEL1);
    dma_set_peripheral_size(DMA2, DMA_CHANNEL1, DMA_CCR_PSIZE_32BIT);
    dma_set_memory_size(DMA2, DMA_CHANNEL1, DMA_CCR_MSIZE_32BIT);
    dma_set_priority(DMA2, DMA_CHANNEL1, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(DMA2, DMA_CHANNEL1);

    dma_enable_channel(DMA2, DMA_CHANNEL1);
}



void sai_init(void)
{
    const port_n_pins_t sai_pins[]  = SAI_PORT_N_PINS;
    const uint32_t      sai_pin_funcs[] = SAI_PORT_N_PINS_AF;

    rcc_periph_clock_enable(SCC_SAI1);

    RCC_PLLSAI1_CFGR &= (RCC_PLLSAIx_CFGR_PLLSAI1PEN | RCC_PLLSAIx_CFGR_PLLSAI1QEN);

    RCC_CR &= ~RCC_CR_PLLSAI1ON;
    while(RCC_CR & RCC_CR_PLLSAI1RDY);

    SAI1_ACR1 = (SAI_xCR1_MODE_MASTER_RECV << SAI_xCR1_MODE_SHIFT)  |
                (SAI_xCR1_PRTCFG_FREE << SAI_xCR1_PRTCFG_SHIFT)    |
                (SAI_xCR1_DS_24BIT << SAI_xCR1_DS_SHIFT)           |
                SAI_xCR1_MONO | SAI_xCR1_LSBFIRST | SAI_xCR1_CKSTR |
                SAI_xCR1_SAIEN | (2 << SAI_xCR1_MCKDIV_SHIFT);

    SAI1_AFRCR = (63 << SAI_xFRCR_FRL_SHIFT) |
                 (31 << SAI_xFRCR_FSALL_SHIFT) |
                 SAI_xFRCR_FSOFF;

    SAI1_ASLOTR = ((0xF << SAI_xSLOTR_NBSLOT_SHIFT) & SAI_xSLOTR_NBSLOT_SHIFT) |
                  ((USED_SLOTS-1) << SAI_xSLOTR_SLOTEN_SHIFT) |
                  (SAI_xSLOTR_SLOTSZ_32BIT << SAI_xSLOTR_SLOTSZ_SHIFT);

/*
• f(VCOSAI1 clock) = f(PLL clock input) × (PLLSAI1N / PLLM)    (VCOSAI1 clock) = 16Mhz * (4 / 4) = 16Mhz
• f(PLLSAI1_P) = f(VCOSAI1 clock) / PLLSAI1P                   PLLSAI1_P = 16Mhz / 2 = 8Mhz
• f(PLLSAI1_Q) = f(VCOSAI1 clock) / PLLSAI1Q
• f(PLLSAI1_R) = f(VCOSAI1 clock) / PLLSAI1R */

    RCC_PLLSAI1_CFGR = (2 << RCC_PLLSAIx_CFGR_PLLSAI1PDIV_SHIFT) | (4 << RCC_PLLSAIx_CFGR_PLLSAI1N_SHIFT) | RCC_PLLSAIx_CFGR_PLLSAI1PEN;

    RCC_CR |= RCC_CR_PLLSAI1ON;
    while(!(RCC_CR & RCC_CR_PLLSAI1RDY));

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

    dma_channel_reset(DMA2, DMA_CHANNEL1);

    dma_set_peripheral_address(DMA2, DMA_CHANNEL1, (uint32_t)&SAI1_ADR);
    dma_set_memory_address(DMA2, DMA_CHANNEL1, (uint32_t)sai_slots);
    dma_set_number_of_data(DMA2, DMA_CHANNEL1, ARRAY_SIZE(sai_slots));
    dma_set_read_from_peripheral(DMA2, DMA_CHANNEL1);
    dma_enable_memory_increment_mode(DMA2, DMA_CHANNEL1);
    dma_set_peripheral_size(DMA2, DMA_CHANNEL1, DMA_CCR_PSIZE_32BIT);
    dma_set_memory_size(DMA2, DMA_CHANNEL1, DMA_CCR_MSIZE_32BIT);
    dma_set_priority(DMA2, DMA_CHANNEL1, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(DMA2, DMA_CHANNEL1);

    dma_enable_channel(DMA2, DMA_CHANNEL1);

    _sai_dma_init();

    SAI1_ACR2 = (SAI_xCR2_FTH_FIFO_1_2 << SAI_xCR2_FTH_SHIFT) | SAI_xCR2_FFLUSH;
    SAI1_AIM  = SAI_xIM_FREQIE;
    SAI1_ACR1 |= SAI_xCR1_DMAEN;
}


void dma2_channel1_isr(void)
{
    DMA2_IFCR |= DMA_IFCR_CTCIF(DMA_CHANNEL1);

    if (audio_dumping)
    {
        uart_ring_out(CMD_VUART, (char *)sai_slots, sizeof(sai_slots)/sizeof(char));
    }

    _sai_dma_init();
}

