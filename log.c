#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

#include <libopencm3/stm32/usart.h>


#include "log.h"
#include "uarts.h"
#include "uart_rings.h"
#include "persist_config.h"
#include "sys_time.h"

uint32_t log_debug_mask = DEBUG_SYS | DEBUG_LW | DEBUG_MEASUREMENTS | DEBUG_LIGHT;
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


void log_debug(uint32_t flag, const char *s, ...)
{
    if (!(flag & log_debug_mask))
        return;

    va_list ap;
    va_start(ap, s);

    char prefix[18];

    snprintf(prefix, sizeof(prefix), "DEBUG:%010u:", (unsigned)since_boot_ms);

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


void log_error(const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(UART_ERR_NU, false, "ERROR:", s, ap);
    va_end(ap);
}


void log_out(const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(CMD_VUART, false, NULL, s, ap);
    va_end(ap);
}


extern void log_init(void)
{
    log_debug_mask = persist_get_log_debug_mask();
}


void log_debug_data(uint32_t flag, void * data, unsigned size)
{
    if ((flag & log_debug_mask) != flag)
        return;
    uint8_t * src = (uint8_t*)data;
    uart_rings_out_drain();
    while(size)
    {
        char * pos = log_buffer;
        snprintf(pos, 12, "0x%08x ", (uintptr_t)src);
        pos+=11;
        unsigned len = (size > 16)?16:size;
        for(unsigned i = 0; i <len; i++, pos+=2)
        {
            uart_rings_out_drain();
            snprintf(pos, 3, "%02"PRIx8, src[i]);
        }
        size -= len;
        src += len;

        _dispatch_line(UART_ERR_NU, false, NULL, 43);

        uart_rings_out_drain();
    }
}


void log_debug_value(uint32_t flag, const char * prefix, value_t * v)
{
    if (!v || !prefix)
        return;

    switch(v->type)
    {
        case VALUE_UINT8  : log_debug(flag, "%s%"PRIu8,  prefix, v->u8);  break;
        case VALUE_INT8   : log_debug(flag, "%s%"PRId8,  prefix, v->i8);  break;
        case VALUE_UINT16 : log_debug(flag, "%s%"PRIu16, prefix, v->u16); break;
        case VALUE_INT16  : log_debug(flag, "%s%"PRId16, prefix, v->i16); break;
        case VALUE_UINT32 : log_debug(flag, "%s%"PRIu32, prefix, v->u32); break;
        case VALUE_INT32  : log_debug(flag, "%s%"PRId32, prefix, v->i32); break;
        case VALUE_UINT64 : log_debug(flag, "%s%"PRIu64, prefix, v->u64); break;
        case VALUE_INT64  : log_debug(flag, "%s%lld",    prefix, v->i64); break;
        case VALUE_FLOAT  : log_debug(flag, "%s%f",      prefix, v->f);   break;
        default: log_debug(flag, "%sINVALID", prefix); break;
    }
}
