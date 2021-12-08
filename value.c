#include <inttypes.h>

#include "value.h"
#include "log.h"




void value_add(value_t * dst, value_t *a, value_t* b)
{
    if (!dst || !a || !b)
        return;

    *dst = value_from(value_get(a) + value_get(b));
}



void value_sub(value_t * dst, value_t *a, value_t* b)
{
    if (!dst || !a || !b)
        return;

    *dst = value_from(value_get(a) - value_get(b));
}


void value_mult(value_t * dst, value_t *a, value_t* b)
{
    if (!dst || !a || !b)
        return;

    *dst = value_from(value_get(a) * value_get(b));
}


void value_div(value_t * dst, value_t *a, value_t* b)
{    if (!dst || !a || !b)
        return;

    *dst = value_from(value_get(a) / value_get(b));
}


bool value_equ(value_t *a, value_t* b)
{
    return (value_get(a) == value_get(b));
}


bool value_grt(value_t *a, value_t* b)
{
    return (value_get(a) > value_get(b));
}


bool value_lst(value_t *a, value_t* b)
{
    return (value_get(a) < value_get(b));
}


void value_log_debug(uint32_t flag, const char * prefix, value_t * v)
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
        case VALUE_DOUBLE : log_debug(flag, "%s%f",      prefix, v->r);   break;
        default: log_debug(flag, "%sINVALID", prefix); break;
    }
}
