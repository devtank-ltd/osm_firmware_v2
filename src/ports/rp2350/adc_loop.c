#include <pico/stdlib.h>
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/regs/dreq.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <stdatomic.h>
#include <inttypes.h>
#include <stdlib.h>

#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"

#include "model_config.h"

#include "osm/sensors/adc_loop.h"


#define ADCS_BUFSIZ                             (1024 * ADCS_CHANNEL_COUNT)
_Static_assert(ADCS_BUFSIZ % ADCS_CHANNEL_COUNT == 0, "Buffer size misaligned");

#define ADCS_TARGET_FREQ_PER_CHAN_HZ            (1000 * 100) /* For CP we need to get 100 samples per wave form */
#define ADCS_TARGET_FREQ_HZ                     (ADCS_TARGET_FREQ_PER_CHAN_HZ * ADCS_CHANNEL_COUNT)
#define ADCS_CLKDIV(_sys_clk)                   ((float)_sys_clk / (float)ADCS_TARGET_FREQ_HZ)

static int _adcs_dma_channel = -1;
static volatile uint16_t _adcs_buf[ADCS_BUFSIZ * 2];


static void _adcs_dma_isr(void)
{
    int dma_chan = _adcs_dma_channel;
    dma_channel_acknowledge_irq0(dma_chan);
    static bool w_pos = true;
    uint16_t* w_buf = (uint16_t*)_adcs_buf;
    volatile uint16_t* r_buf = (volatile uint16_t*)w_buf + ADCS_BUFSIZ;
    if (w_pos) {
        r_buf = (volatile uint16_t*)w_buf;
        w_buf += ADCS_BUFSIZ;
    }
    w_pos = !w_pos;
    dma_channel_set_write_addr(
        dma_chan,
        w_buf,
        true
    );
    /* Now the dma is off running again, start processing the results */

    for (unsigned i = 0; i < ADCS_BUFSIZ; i++) {
        uint16_t v = r_buf[i];
        if ((1 << 15) & v) {
            /* Error bit set, skip */
            continue;
        }
        v &= ((1 << 12) - 1);
        unsigned adc_chan = i % ADCS_CHANNEL_COUNT;

        osm_adc_loop_channel_add(adc_chan, v);
    }

    osm_adc_loop_interate();
}


void osm_adc_loop_init(void)
{
    uint32_t adc_mask = 0;

    for(unsigned n = 0; n <= ADCS_CHANNEL_COUNT; n++)
    {
        adc_gpio_init(ADC_GPIO_BASE + n);
        adc_mask |= 1U << n;
    }

    adc_init();

    uint32_t sys_clk = clock_get_hz(clk_sys);
    adc_set_clkdiv(ADCS_CLKDIV(sys_clk));

    adc_set_round_robin(adc_mask);

    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // Assert DREQ (and IRQ) at least 1 sample present
        true,
        false
    );
    adc_fifo_drain();

    uint dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);

    channel_config_set_dreq(&c, DREQ_ADC);

    dma_channel_configure(dma_chan, &c,
        NULL,
        &adc_hw->fifo,
        ADCS_BUFSIZ,
        false
    );
    dma_channel_set_irq0_enabled(dma_chan, true);

    irq_set_exclusive_handler(DMA_IRQ_0, _adcs_dma_isr);
    irq_set_enabled(DMA_IRQ_0, true);

    _adcs_dma_channel = dma_chan;
    dma_channel_set_write_addr(
        dma_chan,
        _adcs_buf,
        true
    );

    adc_run(true);
}
