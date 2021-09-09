
#include <libopencm3/stm32/gpio.h>
#include "config.h"
#include "pinmap.h"

#define SAI1   SAI1_BASE

#define SAI_CR1(sai, block)    MMIO32(sai + 0x04 + 0x20 * block)
#define SAI_CR2(sai, block)    MMIO32(sai + 0x04 + 0x20 * block)
#define SAI_AFRCR(sai, block)  MMIO32(sai + 0x0C + 0x20 * block)
#define SAI_ASLOTR(sai, block) MMIO32(sai + 0x10 + 0x20 * block)
#define SAI_AIM(sai, block)    MMIO32(sai + 0x14 + 0x20 * block)
#define SAI_ASR(sai, block)    MMIO32(sai + 0x18 + 0x20 * block)
#define SAI_ACLRFR(sai, block) MMIO32(sai + 0x1C + 0x20 * block)
#define SAI_ADR(sai, block)    MMIO32(sai + 0x20 + 0x20 * block)

#define SAI1_ACR1   SAI_CR1(SAI1, 0)
#define SAI1_ACR2   SAI_CR2(SAI1, 0)
#define SAI1_AFRCR  SAI_FRCR(SAI1, 0)
#define SAI1_ASLOTR SAI_SLOTR(SAI1, 0)
#define SAI1_AIM    SAI_IM(SAI1, 0)
#define SAI1_ASR    SAI_SR(SAI1, 0)
#define SAI1_ACLRFR SAI_CLRFR(SAI1, 0)
#define SAI1_ADR    SAI_DR(SAI1, 0)

#define SAI1_BCR1   SAI_CR1(SAI1, 1)
#define SAI1_BCR2   SAI_CR2(SAI1, 1)
#define SAI1_BFRCR  SAI_FRCR(SAI1, 1)
#define SAI1_BSLOTR SAI_SLOTR(SAI1, 1)
#define SAI1_BIM    SAI_IM(SAI1, 1)
#define SAI1_BSR    SAI_SR(SAI1, 1)
#define SAI1_BCLRFR SAI_CLRFR(SAI1, 1)
#define SAI1_BDR    SAI_DR(SAI1, 1)


#define SAI_CR1_MCKDIV_SHIFT   20
#define SAI_CR1_MCKDIV_MASK    0xF
#define SAI_CR1_NODIV          (1 << 19)
#define SAI_CR1_DMAEN          (1 << 17)
#define SAI_CR1_SAIEN          (1 << 16)
#define SAI_CR1_OUTDRIV        (1 << 13)
#define SAI_CR1_MONO           (1 << 12)
#define SAI_CR1_SYNCEN_SHIFT   10
#define SAI_CR1_SYNCEN_MASK    0x3
#define SAI_CR1_CKSTR          (1 << 9)
#define SAI_CR1_LSBFIRST       (1 << 8)
#define SAI_CR1_DS_SHIFT       5
#define SAI_CR1_DS_MASK        0x7
#define SAI_CR1_PRTCFG_SHIFT   3
#define SAI_CR1_PRTCFG_MASK    0x3
#define SAI_CR1_MODE_SHIFT     0
#define SAI_CR1_MODE_MASK      0x3


enum sai_cr1_syncen {
    SAI_CR1_SYNCEN_ASYNC = 0,
    SAI_CR1_SYNCEN_SYNC  = 1,
};

enum sai_cr1_ds {
    SAI_CR1_DS_8BIT  = 2,
    SAI_CR1_DS_10BIT = 3,
    SAI_CR1_DS_16BIT = 4,
    SAI_CR1_DS_20BIT = 5,
    SAI_CR1_DS_24BIT = 6,
    SAI_CR1_DS_32BIT = 7,
};

enum sai_cr1_prtcfg {
    SAI_CR1_PRTCFG_FREE  = 0,
    SAI_CR1_PRTCFG_SPDIF = 1,
    SAI_CR1_PRTCFG_AC97  = 2,
};

enum sai_cr1_mode {
    SAI_CR1_MODE_MASTER_TRANS = 0,
    SAI_CR1_MODE_MASTER_RECV = 1,
    SAI_CR1_MODE_SLAVE_TRANS = 2,
    SAI_CR1_MODE_SLAVE_RECV = 3,
};


#define SAI_CR2_COMP_MASK     0x3
#define SAI_CR2_COMP_SHIFT    14
#define SAI_CR2_CPL           (1 << 13)
#define SAI_CR2_MUTECNT_MASK  0x3f
#define SAI_CR2_MUTECNT_SHIFT 7
#define SAI_CR2_MUTEVAL       (1 << 6)
#define SAI_CR2_MUTE          (1 << 5)
#define SAI_CR2_TRIS          (1 << 4)
#define SAI_CR2_FFLUSH        (1 << 3)
#define SAI_CR2_FTH_MASK      (0x3)
#define SAI_CR2_FTH_SHIFT     0

enum sai_cr2_fth {
    SAI_CR2_FTH_FIFO_EMPTY = 0,
    SAI_CR2_FTH_FIFO_1_4   = 1,
    SAI_CR2_FTH_FIFO_1_2   = 2,
    SAI_CR2_FTH_FIFO_3_4   = 3,
    SAI_CR2_FTH_FIFO_FULL  = 4,
};



void sai_cr1_set(uint32_t sai, uint8_t block, uint8_t mckdiv, bool nodiv,
                 bool dmaen, bool saien, bool outdriv, bool mono,
                 enum sai_cr1_syncen syncen, bool ckstr, bool lsbfirst, enum sai_cr1_ds ds,
                 enum sai_cr1_prtcfg prtcfg, enum sai_cr1_mode mode)
{
    SAI_CR1(sai, block) = ((mckdiv & SAI_CR1_MCKDIV_MASK) << SAI_CR1_MCKDIV_SHIFT) |
                          ((nodiv)?SAI_CR1_NODIV:0) |
                          ((dmaen)?SAI_CR1_DMAEN:0) |
                          ((saien)?SAI_CR1_SAIEN:0) |
                          ((outdriv)?SAI_CR1_OUTDRIV:0) |
                          ((mono)?SAI_CR1_MONO:0) |
                          ((((uint8_t)syncen) & SAI_CR1_SYNCEN_MASK) << SAI_CR1_SYNCEN_SHIFT) |
                          ((ckstr)?SAI_CR1_CKSTR:0) |
                          ((lsbfirst)?SAI_CR1_LSBFIRST:0) |
                          ((((uint8_t)ds) & SAI_CR1_DS_MASK) << SAI_CR1_DS_SHIFT) |
                          ((((uint8_t)prtcfg) & SAI_CR1_PRTCFG_MASK) << SAI_CR1_PRTCFG_SHIFT) |
                          ((((uint8_t)mode) & SAI_CR1_MODE_MASK) << SAI_CR1_MODE_SHIFT);
}


void sai_cr2_set(uint32_t sai, uint8_t block, uint8_t comp, bool cpl,
                 uint8_t mutecnt, bool muteval, bool mute, bool tris,
                 bool fflush, enum sai_cr2_fth fth)
{
    SAI_CR2(sai, block) = ((comp & SAI_CR2_COMP_MASK) << SAI_CR2_COMP_SHIFT) |
                          ((cpl)?SAI_CR2_CPL:0) |
                          ((mutecnt & SAI_CR2_MUTECNT_MASK) << SAI_CR2_MUTECNT_SHIFT) |
                          ((muteval)?SAI_CR2_MUTEVAL:0) |
                          ((mute)?SAI_CR2_MUTE:0) |
                          ((tris)?SAI_CR2_TRIS:0) |
                          ((fflush)?SAI_CR2_FFLUSH:0) |
                          ((((uint8_t)fth) & SAI_CR2_FTH_MASK) << SAI_CR2_FTH_SHIFT);
}





void sai_init(void)
{
    const port_n_pins_t sai_pins[]  = SAI_PORT_N_PINS;
    const uint32_t      sai_pin_funcs[] = SAI_PORT_N_PINS_AF;

    for(unsigned n = 0; n < ARRAY_SIZE(sai_pins); n++)
    {
        port_n_pins_t sai_pin = sai_pins[n];

        rcc_periph_clock_enable(PORT_TO_RCC(sai_pin.port));
    
        rcc_periph_clock_enable(PORT_TO_RCC(sai_pin.port));
        gpio_mode_setup(sai_pins[n].port,
                        GPIO_MODE_AF,
                        GPIO_PUPD_NONE,
                        sai_pins[n].pins);

        gpio_set_af( sai_pin.port, sai_pin_funcs[n], sai_pin.pins );
    }
}
