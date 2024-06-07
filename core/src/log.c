#include <string.h>
#include <stdio.h>
#include <inttypes.h>


#include "log.h"
#include "uarts.h"
#include "uart_rings.h"
#include "persist_config.h"
#include "common.h"

uint32_t log_debug_mask = DEBUG_SYS | DEBUG_COMMS | DEBUG_MEASUREMENTS | DEBUG_LIGHT;
bool     log_async_log  = false;


extern void platform_raw_msg(const char * s)
{
    uart_blocking(UART_ERR_NU, s, strlen(s));
    uart_blocking(UART_ERR_NU, "\n\r", 2);
}

static char log_buffer[LOG_LINELEN];


static void _dispatch_line(unsigned uart, bool blocking, const char * prefix, unsigned len)
{
    if (!blocking)
    {
        if (prefix)
            uart_ring_out(uart, prefix, strlen(prefix));
        uart_ring_out(uart, log_buffer, len);
        uart_ring_out(uart, "\n\r", 2);
    }
    else
    {
        if (prefix)
            uart_blocking(uart, prefix, strlen(prefix));
        uart_blocking(uart, log_buffer, len);
        uart_blocking(uart, "\n\r", 2);
    }
}


static void log_msgv(unsigned uart, bool blocking, const char * prefix, const char *s, va_list ap)
{
    unsigned len = vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;
    if (len > LOG_LINELEN-1)
        len = LOG_LINELEN-1;

    _dispatch_line(uart, blocking || !log_async_log, prefix, len);
}


void log_debugv(uint32_t flag, const char * s,va_list ap)
{
    if (!(flag & log_debug_mask))
        return;

    char prefix[18];

    snprintf(prefix, sizeof(prefix), "DEBUG:%010u:", (unsigned)get_since_boot_ms());

    log_msgv(UART_ERR_NU, false, prefix, s, ap);
}


void log_debug(uint32_t flag, const char *s, ...)
{
    if (!(flag & log_debug_mask))
        return;

    va_list ap;
    va_start(ap, s);

    char prefix[18];

    snprintf(prefix, sizeof(prefix), "DEBUG:%010u:", (unsigned)get_since_boot_ms());

    log_msgv(UART_ERR_NU, false, prefix, s, ap);
    va_end(ap);
}


void log_bad_error(const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    /*We block write errors as we don't know what is broken at this point.*/
    log_msgv(UART_ERR_NU, true, "ERROR:", s, ap);
    va_end(ap);
}


void log_errorv(const char * s, va_list ap)
{
    log_msgv(UART_ERR_NU, false, "ERROR:", s, ap);
}


void log_error(const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_errorv(s, ap);
    va_end(ap);
}


void log_outv(const char * s, va_list ap)
{
    log_msgv(CMD_VUART, false, NULL, s, ap);
}


void log_out(const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_outv(s, ap);
    va_end(ap);
}


extern void log_init(void)
{
    log_debug_mask = persist_get_log_debug_mask();
}


void log_debug_data(uint32_t flag, const void * data, unsigned size)
{
    if ((flag & log_debug_mask) != flag)
        return;
    const uint8_t * src_start = (const uint8_t*)data;
    const uint8_t * src = src_start;
    snprintf(log_buffer, LOG_LINELEN, "Start %p", src_start);
    _dispatch_line(UART_ERR_NU, false, NULL, strnlen(log_buffer, LOG_LINELEN-1));
    uart_rings_out_drain();
    while(size)
    {
        char * pos = log_buffer;
        snprintf(pos, 12, "0x%08"PRIxPTR" ", (uintptr_t)(src - src_start));
        pos+=11;
        unsigned len = (size > 16)?16:size;
        for(unsigned i = 0; i <len; i++, pos+=2)
        {
            uart_rings_out_drain();
            snprintf(pos, 3, "%02"PRIx8, src[i]);
        }
        size -= len;
        src += len;

        _dispatch_line(UART_ERR_NU, false, NULL, pos - log_buffer);

        uart_rings_out_drain();
    }
}
