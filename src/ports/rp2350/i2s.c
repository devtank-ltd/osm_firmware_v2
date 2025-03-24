#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/divider.h"
#include "pico/sync.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

#include "i2s.pio.h"

#include <osm/core/base_types.h>
#include <osm/core/common.h>
#include <osm/core/log.h>
#include "model_pinmap.h"

#ifndef PRIi64
#define PRIi64 "lld"
#endif
#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRIX64
#define PRIX64 "llX"
#endif

#define I2S_BITDEPTH            32

#define I2S_AUDIO_BUFFER_FRAMES 48
#define I2S_STEREO_BUFFER_SIZE  (I2S_AUDIO_BUFFER_FRAMES * 2)
#define I2S_OUTPUT_BUF_SIZ      (I2S_STEREO_BUFFER_SIZE * 2)
#define I2S_INPUT_BUF_SIZ       (I2S_STEREO_BUFFER_SIZE * 2)

typedef enum {
    DMA_CH_IN_CTRL = 0,
    DMA_CH_IN_DATA = 3,
    DMA_CH_COUNT,
} dma_ch_t;

static void _i2s_dma_setup(PIO pio, uint32_t in_sm, uint32_t out_sm);
static void _i2s_dma_irq(void);
static void _i2s_process_audio(const int32_t* input, size_t num_frames);
static command_response_t _i2s_get_audio_cb(char* args, cmd_ctx_t * ctx);
static void _i2s_get_audio(int32_t* left, int32_t* right);

static int _dma_chs[DMA_CH_COUNT] = {-1};
static int32_t _input_buf[I2S_INPUT_BUF_SIZ];
static int32_t* _dma_in_ctrl_blks[2] = { NULL };
static volatile int64_t _avg[2] = {0};
static volatile uint64_t _sum_square[2] = {0};
static mutex_t _mtx;


void i2s_init(void)
{
    mutex_init(&_mtx);

    PIO pio = pio1;
    uint32_t in_sm  = pio_claim_unused_sm(pio, true);
    uint32_t in_offset = pio_add_program(pio, &i2s_in_program);
    i2s_in_program_init(pio, in_sm, in_offset, I2S_DIN_PIN, I2S_BCK_PIN, I2S_LRCL_PIN);

    uint32_t out_sm  = pio_claim_unused_sm(pio, true);
    uint32_t out_offset = pio_add_program(pio, &i2s_out_program);
    i2s_out_program_init(
        pio, out_sm, out_offset, I2S_BITDEPTH, I2S_BCK_PIN, I2S_LRCL_PIN
    );
    _i2s_dma_setup(pio, in_sm, out_sm);

    uint32_t sm_mask = (1 << in_sm) | (1 << out_sm);
    pio_enable_sm_mask_in_sync(pio, sm_mask);
}

struct cmd_link_t* i2s_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "get_audio", "Set audio.", _i2s_get_audio_cb, false, NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}

static command_response_t _i2s_get_audio_cb(char* args, cmd_ctx_t * ctx)
{
    int32_t left = 0, right = 0;
    _i2s_get_audio(&left, &right);
    cmd_ctx_out(ctx,"L: %"PRId32" R: %"PRId32, left, right);
    return COMMAND_RESP_OK;
}

static void _i2s_get_audio(int32_t* left, int32_t* right)
{
    int32_t* rmss[2];
    rmss[0] = left;
    rmss[1] = right;
    int64_t avg[2] = {0};
    uint64_t sumsqr[2] = {0};

    mutex_enter_blocking(&_mtx);
    memcpy(avg, (void*)_avg, sizeof(_avg));
    memcpy(sumsqr, (void*)_sum_square, sizeof(_sum_square));
    mutex_exit(&_mtx);

#define __PROCESS_AUDIO_FIN_RMS(_index)                                                 \
    {                                                                                   \
        sumsqr[_index] = div_u64u64(sumsqr[_index], (uint64_t)I2S_AUDIO_BUFFER_FRAMES); \
        sumsqr[_index] = sqrt(sumsqr[_index]);                                          \
        int64_t w = (int64_t)sumsqr[_index] + avg[_index];                              \
        *(rmss[_index]) = (int32_t)w;                                                   \
    }

    __PROCESS_AUDIO_FIN_RMS(0)
    __PROCESS_AUDIO_FIN_RMS(1)
}

static void _i2s_dma_setup(PIO pio, uint32_t in_sm, uint32_t out_sm)
{
    for (unsigned i = 0; i < DMA_CH_COUNT; i++)
    {
        _dma_chs[i] = dma_claim_unused_channel(true);
    }

    _dma_in_ctrl_blks[0] = _input_buf;
    _dma_in_ctrl_blks[1] = &_input_buf[I2S_STEREO_BUFFER_SIZE];

    dma_channel_config c = dma_channel_get_default_config(_dma_chs[DMA_CH_IN_CTRL]);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, 3);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    dma_channel_configure(
        _dma_chs[DMA_CH_IN_CTRL],
        &c,
        &dma_hw->ch[_dma_chs[DMA_CH_IN_DATA]].al2_write_addr_trig,
        _dma_in_ctrl_blks,
        1,
        false
    );

    c = dma_channel_get_default_config(_dma_chs[DMA_CH_IN_DATA]);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_chain_to(&c, _dma_chs[DMA_CH_IN_CTRL]);
    channel_config_set_dreq(&c, pio_get_dreq(pio, in_sm, false));

    dma_channel_configure(
        _dma_chs[DMA_CH_IN_DATA],
        &c,
        NULL,                         // Will be set by ctrl chan
        &pio->rxf[in_sm],             // Source pointer
        I2S_STEREO_BUFFER_SIZE,       // Number of transfers
        false                         // Don't start yet
    );

    // Input channel triggers the DMA interrupt handler, hopefully these stay
    // in perfect sync with the output.
    dma_channel_set_irq0_enabled(_dma_chs[DMA_CH_IN_DATA], true);
    irq_set_exclusive_handler(DMA_IRQ_0, _i2s_dma_irq);
    irq_set_enabled(DMA_IRQ_0, true);

    // Enable all the dma channels
    uint32_t dma_mask = 1 << _dma_chs[DMA_CH_IN_CTRL];
    dma_start_channel_mask(dma_mask);
}

static void _i2s_dma_irq(void)
{
    if (*(int32_t**)dma_hw->ch[_dma_chs[DMA_CH_IN_CTRL]].read_addr == _input_buf)
    {
        // It is inputting to the second buffer so we can overwrite the first
        _i2s_process_audio(_input_buf, I2S_AUDIO_BUFFER_FRAMES);
    }
    else
    {
        // It is currently inputting the first buffer, so we write to the second
        _i2s_process_audio(&_input_buf[I2S_STEREO_BUFFER_SIZE], I2S_AUDIO_BUFFER_FRAMES);
    }
    dma_channel_acknowledge_irq0(_dma_chs[DMA_CH_IN_DATA]);
}

static void _i2s_process_audio(const int32_t* input, size_t num_frames)
{
    /* Will potentially miss whole frames, if in the middle of a copy on
     * the main thread, but this is better than bad data (with no mutex)
     * or missing other interrupts (with critical_section).
     *
     * Delaying interrupts perhaps is as bad as missing them, as some
     * other interrupts may require exact timing and having them late
     * may mean their function is invalid. This way they have no problem
     * executing if they don't share this mutex.
     */
    if (mutex_try_enter(&_mtx, NULL))
    {
        int64_t avg[2] = {0};
        uint8_t index = 0;
        for (size_t i = 0; i < num_frames * 2; i++)
        {
            int32_t val = input[i];
            if (val & 0xC00000)
            {
                val |= 0xFF000000;
            }
            avg[index] += val;
            index = !index;
        }
        avg[0] = div_s64s64(avg[0], num_frames);
        avg[1] = div_s64s64(avg[1], num_frames);
        memcpy((void*)_avg, avg, sizeof(_avg));
        uint64_t sumsqr[2] = {0};
        index = 0;
        for (size_t i = 0; i < num_frames * 2; i++)
        {
            int64_t v = input[i];
            if (v & 0xC00000)
            {
                v |= 0xFFFFFFFFFF000000;
            }
            v -= avg[index];
            v *= v;
            sumsqr[index] += (uint64_t)v;
            index = !index;
        }
        memcpy((void*)_sum_square, sumsqr, sizeof(_sum_square));
        mutex_exit(&_mtx);
    }
}
