
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


void sai_cr1_set(uint32_t sai, uint8_t block, uint8_t mckdiv, bool nodiv,
                 bool dmaen, bool saien, bool outdriv, bool mono,
                 uint8_t syncen, bool ckstr, bool lsbfirst, uint8_t ds,
                 uint8_t prtcfg, uint8_t mode)
{
    SAI_CR1(sai, block) = ((mckdiv & SAI_CR1_MCKDIV_MASK) << SAI_CR1_MCKDIV_SHIFT) |
                          ((nodiv)?SAI_CR1_NODIV:0) |
                          ((dmaen)?SAI_CR1_DMAEN:0) |
                          ((saien)?SAI_CR1_SAIEN:0) |
                          ((outdriv)?SAI_CR1_OUTDRIV:0) |
                          ((mono)?SAI_CR1_MONO:0) |
                          ((syncen & SAI_CR1_SYNCEN_MASK) << SAI_CR1_SYNCEN_SHIFT) |
                          ((ckstr)?SAI_CR1_CKSTR:0) |
                          ((lsbfirst)?SAI_CR1_LSBFIRST:0) |
                          ((ds & SAI_CR1_DS_MASK) << SAI_CR1_DS_SHIFT) |
                          ((prtcfg & SAI_CR1_PRTCFG_MASK) << SAI_CR1_PRTCFG_SHIFT) |
                          ((mode & SAI_CR1_MODE_MASK) << SAI_CR1_MODE_SHIFT);
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
