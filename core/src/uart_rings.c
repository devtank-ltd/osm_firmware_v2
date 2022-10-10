#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "uart_rings.h"
#include "ring.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "comms.h"
#include "hpm.h"
#include "modbus.h"
#include "platform.h"

#include "common.h"


typedef char dma_uart_buf_t[DMA_DATA_PCK_SZ];

char uart_0_in_buf[UART_0_IN_BUF_SIZE];
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];

char uart_1_in_buf[UART_1_IN_BUF_SIZE];
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];

char uart_2_in_buf[UART_2_IN_BUF_SIZE];
char uart_2_out_buf[UART_2_OUT_BUF_SIZE];

char uart_3_in_buf[UART_3_IN_BUF_SIZE];
char uart_3_out_buf[UART_3_OUT_BUF_SIZE];


static ring_buf_t ring_in_bufs[UART_CHANNELS_COUNT] = {
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),
    RING_BUF_INIT(uart_2_in_buf, sizeof(uart_2_in_buf)),
    RING_BUF_INIT(uart_3_in_buf, sizeof(uart_3_in_buf)),
    };

static ring_buf_t ring_out_bufs[UART_CHANNELS_COUNT] = {
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),
    RING_BUF_INIT(uart_2_out_buf, sizeof(uart_2_out_buf)),
    RING_BUF_INIT(uart_3_out_buf, sizeof(uart_3_out_buf)),
    };

static char command[CMD_LINELEN];

static dma_uart_buf_t uart_dma_buf[UART_CHANNELS_COUNT];


bool uart_ring_out_busy(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_out_bufs[uart];
    return ring_buf_get_pending(ring);
}


bool uart_rings_out_busy(void)
{
    for (unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        if (uart_ring_out_busy(n))
            return true;
    }
    return false;
}


unsigned uart_ring_in(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_in_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(ring, s[n]))
        {
            log_error("UART-in %u full", uart);
            return n;
        }
    return len;
}


unsigned uart_ring_out(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_out_bufs[uart];

    if (ring->size == 1)
    {
        if (!uart_is_tx_empty(uart))
        {
            log_error("UART %u already DMAing without ring.", uart);
            return 0;
        }

        if (len > DMA_DATA_PCK_SZ)
        {
            log_error("String too big for UART %u DMA without ring.", uart);
            return 0;
        }

        char *dma_mem = uart_dma_buf[uart];

        memcpy(dma_mem, s, len);

        uart_dma_out(uart, dma_mem, len);

        return len;
    }

    if (uart) // Add debug messages to debug ring buffer is a loop.
        log_debug(DEBUG_UART(uart), "UART %u out < %u", uart, len);

    for (unsigned n = 0; n < len; n++)
    {
        if (!ring_buf_add(ring, s[n]))
        {
            if (uart)
                log_error("UART-out %u full", uart);
            else
                platform_raw_msg("Debug UART ring full!");
            return n;
        }
    }
    return len;
}


void uart_ring_in_drain(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    ring_buf_t * ring = &ring_in_bufs[uart];

    if (uart == RS485_UART)
    {
        modbus_ring_process(ring);
        return;
    }

    unsigned len = ring_buf_get_pending(ring);

    if (!len)
        return;

    if (uart)
        log_debug(DEBUG_UART(uart), "UART %u IN %u", uart, len);

    if (uart == CMD_UART)
    {
        len = ring_buf_readline(ring, command, CMD_LINELEN);

        if (len)
            cmds_process(command, len);
    }
    else if (uart == LW_UART)
    {
        len = ring_buf_readline(ring, command, CMD_LINELEN);
        if (len)
        {
            comms_debug(" >> %s", command);
            comms_process(command);
        }
    }
    else if (uart == HPM_UART)
    {
        hpm_ring_process(ring, command, CMD_LINELEN);
    }
}


static unsigned _uart_out_dma(char * c, unsigned len, void * puart)
{
    unsigned uart = *(unsigned*)puart;

    if (uart_dma_out(uart, c, len))
        return len;
    return 0;
}


static void uart_ring_out_drain(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    ring_buf_t * ring = &ring_out_bufs[uart];

    unsigned len = ring_buf_get_pending(ring);

    if (uart == RS485_UART)
    {
        static bool rs485_transmitting = false;
        static uint32_t rs485_start_transmitting = 0;

        if (!len)
        {
            if (rs485_transmitting && uart_is_tx_empty(RS485_UART))
            {
                static bool rs485_transmit_stopping = false;
                static uint32_t rs485_stop_transmitting = 0;
                if (!rs485_transmit_stopping)
                {
                    modbus_debug("Sending complete, delay %"PRIu32"ms", modbus_stop_delay());
                    rs485_stop_transmitting = get_since_boot_ms();
                    rs485_transmit_stopping = true;
                    return;
                }
                else if (since_boot_delta(get_since_boot_ms(), rs485_stop_transmitting) > modbus_stop_delay())
                {
                    rs485_transmitting = false;
                    rs485_transmit_stopping = false;
                    platform_set_rs485_mode(false);
                    return;
                }
            }
            return;
        }

        if (!rs485_transmitting)
        {
            rs485_transmitting = true;
            platform_set_rs485_mode(true);
            rs485_start_transmitting = get_since_boot_ms();
            modbus_debug("Data to send, delay %"PRIu32"ms", modbus_start_delay());
            return;
        }
        else
        {
            if (since_boot_delta(get_since_boot_ms(), rs485_start_transmitting) < modbus_start_delay())
                return;
        }

        modbus_debug("Sending %u", len);
    }
    else if (!len)
        return;

    if (uart_is_tx_empty(uart))
    {
        if (uart)
            log_debug(DEBUG_UART(uart), "UART %u OUT > %u", uart, len);

        len = (len > DMA_DATA_PCK_SZ)?DMA_DATA_PCK_SZ:len;

        ring_buf_consume(ring, _uart_out_dma, uart_dma_buf[uart], len, &uart);
    }
}


void uart_rings_in_drain()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_ring_in_drain(n);
}


void uart_rings_out_drain()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_ring_out_drain(n);
}


static void uart_ring_check(ring_buf_t * ring, char * name, unsigned index)
{
    log_debug(DEBUG_UART(index), "UART %u %s r_pos %u", index, name, ring->r_pos);
    log_debug(DEBUG_UART(index), "UART %u %s w_pos %u", index, name, ring->w_pos);
    log_debug(DEBUG_UART(index), "UART %u %s pending %u", index, name, ring_buf_get_pending(ring));

    if (ring_buf_is_full(ring))
        log_error("UART %u %s ring buffer filled.", index, name);
}


void uart_rings_check()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        ring_buf_t * in_ring  = &ring_in_bufs[n];
        ring_buf_t * out_ring = &ring_out_bufs[n];

        uart_ring_check(in_ring, "in", n);
        uart_ring_check(out_ring, "out", n);
    }
}


void uart_rings_init(void)
{
}
