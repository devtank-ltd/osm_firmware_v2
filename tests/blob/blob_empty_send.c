/*
 * blob_empty_send.c
 *
 * Copyright 2023 Harry Geyer <hgeyer@devtop>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */


#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "log.h"


static bool (*_test_comms_send)(int8_t* hex_arr, uint16_t arr_len) = NULL;
static void (*_log_error)(char * s) = NULL;
static void (*_log_debug)(uint32_t flag, char * s) = NULL;


void test_comms_send_set_cb(bool (*cb)(int8_t* hex_arr, uint16_t arr_len))
{
    _test_comms_send = cb;
}


void log_error_set_cb(void (*cb)(char * s))
{
    _log_error = cb;
}


void log_debug_set_cb(void (*cb)(uint32_t flag, char * s))
{
    _log_debug = cb;
}


static int _log(const char* prefix, const char * s, va_list ap, char** buf)
{
    unsigned prefix_len = strlen(prefix);
    unsigned len = vsnprintf(NULL, 0, s, ap);
    *buf = malloc(len + prefix_len + 1);
    if (!buf)
    {
        /* Ran out of memory */
        return -1;
    }
    strncpy(*buf, prefix, prefix_len);
    len = vsnprintf(*buf + prefix_len, len+1, s, ap);
    buf[len + prefix_len + 1] = 0;
    return len;
}


void PRINTF_FMT_CHECK( 1, 2) log_error(const char * s, ...)
{
    if (_log_error)
    {
        va_list ap;
        va_start(ap, s);
        char* buf = NULL;
        _log("ERROR:", s, ap, &buf);
        _log_error(buf);
        free(buf);
    }
}


void PRINTF_FMT_CHECK( 2, 3) log_debug(uint32_t flag, const char * s, ...)
{
    if (_log_debug)
    {
        va_list ap;
        va_start(ap, s);
        char* buf = NULL;
        _log("DEBUG:", s, ap, &buf);
        _log_debug(flag, buf);
        free(buf);
    }
}


bool test_comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    if (_test_comms_send)
    {
        return _test_comms_send(hex_arr, arr_len);
    }
    return false;
}

