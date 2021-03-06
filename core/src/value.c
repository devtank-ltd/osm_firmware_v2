#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#include "value.h"


bool value_as_str(value_t* value, char* d, unsigned len)
{
    if (len > VALUE_STR_LEN)
    {
        return false;
    }
    memset(value->str, 0, VALUE_STR_LEN*sizeof(char));
    memcpy(value->str, d, len*sizeof(char));
    value->type = VALUE_STR;
    return true;
}


static value_type_t _get_op_max_type(value_t *a, value_t* b)
{
    if (!a || a->type == VALUE_UNSET || a->type == VALUE_STR ||
        !b || b->type == VALUE_UNSET || b->type == VALUE_STR)
        return VALUE_UNSET;

    if (a->type == VALUE_FLOAT || b->type == VALUE_FLOAT)
        return VALUE_FLOAT;

    return VALUE_INT64;
}


void value_compact(value_t *v)
{
    if (!v || v->type >= VALUE_FLOAT)
        return;

    if ((v->type == VALUE_UINT64) && (v->u64 & 0x8000000000000000))
        return;

    int64_t i = value_get(v);
    v->i64 = i;
    if (i >= 0)
    {
        /* Drop signed */
        uint64_t u = i;
        if (u < (1ULL << 32))
        {
            if (u < (1ULL << 16))
            {
                if (u < (1ULL << 8))
                {
                    v->type = VALUE_UINT8;
                    return;
                }
                v->type = VALUE_UINT16;
                return;
            }
            v->type = VALUE_UINT32;
            return;
        }
        v->type = VALUE_UINT64;
    }
    else
    {
        if (i >= -(1LL << 31))
        {
            if (i >= -(1LL << 15))
            {
                if (i >= -(1LL << 7))
                {
                    v->type = VALUE_INT8;
                    return;
                }
                v->type = VALUE_INT16;
                return;
            }
            v->type = VALUE_INT32;
            return;
        }
        v->type = VALUE_INT64;
    }
}


#define _value_op(dst, a, b, _op_)                     \
{                                                      \
    value_type_t type = _get_op_max_type(a, b);        \
                                                       \
    if (!dst)                                          \
        return false;                                  \
                                                       \
    dst->type = type;                                  \
    if (type == VALUE_UNSET)                           \
        return false;                                  \
                                                       \
    switch(type)                                       \
    {                                                  \
       case VALUE_INT64:                               \
        {                                              \
            dst->i64 = value_get(a) _op_ value_get(b); \
            break;                                     \
        }                                              \
        case VALUE_FLOAT:                              \
        {                                              \
            dst->f = value_get(a) _op_ value_get(b);   \
            break;                                     \
        }                                              \
        default:                                       \
            return false;                              \
    }                                                  \
                                                       \
    value_compact(dst);                                \
    return true;                                       \
}



bool value_add(value_t * dst, value_t *a, value_t* b)
{
    _value_op(dst, a, b, +);
}


bool value_sub(value_t * dst, value_t *a, value_t* b)
{
    _value_op(dst, a, b, -);
}


bool value_mult(value_t * dst, value_t *a, value_t* b)
{
    _value_op(dst, a, b, *);
}


bool value_div(value_t * dst, value_t *a, value_t* b)
{
    _value_op(dst, a, b, /);
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


bool value_from_str(value_t * v, char * str)
{
    if (!v || !str)
        return false;

    v->type = VALUE_UNSET;

    char * splitter = NULL;

    unsigned bits = strtoul(str + 1, &splitter, 10);

    if (!splitter || splitter[0] != ':')
        return false;

    switch(tolower(str[0]))
    {
        case 'u':
            switch(bits)
            {
                case 8:  v->type = VALUE_UINT8;  v->u8  = strtoul(splitter + 1, NULL, 10); return true;
                case 16: v->type = VALUE_UINT16; v->u16 = strtoul(splitter + 1, NULL, 10); return true;
                case 32: v->type = VALUE_UINT32; v->u32 = strtoul(splitter + 1, NULL, 10); return true;
                case 64: v->type = VALUE_UINT64; v->u64 = strtoul(splitter + 1, NULL, 10); return true;
                default:
                    return false;
            }
            break;
        case 'i':
            switch(bits)
            {
                case 8:  v->type = VALUE_INT8;  v->i8  = strtol(splitter + 1, NULL, 10); return true;
                case 16: v->type = VALUE_INT16; v->i16 = strtol(splitter + 1, NULL, 10); return true;
                case 32: v->type = VALUE_INT32; v->i32 = strtol(splitter + 1, NULL, 10); return true;
                case 64: v->type = VALUE_INT64; v->i64 = strtol(splitter + 1, NULL, 10); return true;
                default:
                    return false;
            }
            break;
        case 'f':
            switch(bits)
            {
                case 32: v->type = VALUE_FLOAT;  v->f  = strtof(splitter + 1, NULL); return true;
                default:
                    return false;
            }
        default:
            return false;
    }
    return false;
}


bool value_to_str(value_t * v, char * str, unsigned size)
{
    if (!v || !str || !size)
        return false;

    switch(v->type)
    {
        case VALUE_UINT8  : snprintf(str, size, "U8:%"PRIu8,   v->u8);  break;
        case VALUE_UINT16 : snprintf(str, size, "U16:%"PRIu16, v->u16); break;
        case VALUE_UINT32 : snprintf(str, size, "U32:%"PRIu32, v->u32); break;
        case VALUE_UINT64 : snprintf(str, size, "U64:%"PRIu64, v->u64); break;
        case VALUE_INT8   : snprintf(str, size, "I8:%"PRId8,   v->i8);  break;
        case VALUE_INT16  : snprintf(str, size, "I16:%"PRId16, v->i16); break;
        case VALUE_INT32  : snprintf(str, size, "I32:%"PRId32, v->i32); break;
        case VALUE_INT64  : snprintf(str, size, "I64:%"PRId64, v->i64); break;
        case VALUE_FLOAT  : snprintf(str, size, "F32:%f",      v->f);   break;
        case VALUE_STR    : snprintf(str, size, "STR:%s",      v->str); break;
        default: return false;
    }
    return true;
}
