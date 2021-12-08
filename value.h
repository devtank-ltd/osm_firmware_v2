#pragma once
#include <stdint.h>

typedef enum
{
    VALUE_UNSET,
    VALUE_UINT8,
    VALUE_INT8,
    VALUE_UINT16,
    VALUE_INT16,
    VALUE_UINT32,
    VALUE_INT32,
    VALUE_UINT64,
    VALUE_INT64,
    VALUE_FLOAT,
    VALUE_DOUBLE,
} value_type_t;

typedef struct
{
    value_type_t type;
    union
    {
        uint8_t u8;
        int8_t  i8;
        uint8_t u16;
        int8_t  i16;
        uint8_t u32;
        int8_t  i32;
        uint8_t u64;
        int8_t  i64;
        float   f;
        double  r;
    };
} value_t;

#define VALUE_FROM_U8(_d_)     (value_t){.type = VALUE_UINT8,  .u8  = _d_}
#define VALUE_FROM_I8(_d_)     (value_t){.type = VALUE_INT8,   .i8  = _d_}
#define VALUE_FROM_U16(_d_)    (value_t){.type = VALUE_UINT16, .u16 = _d_}
#define VALUE_FROM_I16(_d_)    (value_t){.type = VALUE_INT16,  .i16 = _d_}
#define VALUE_FROM_U32(_d_)    (value_t){.type = VALUE_UINT32, .u32 = _d_}
#define VALUE_FROM_I32(_d_)    (value_t){.type = VALUE_INT32,  .i32 = _d_}
#define VALUE_FROM_U64(_d_)    (value_t){.type = VALUE_UINT64, .u64 = _d_}
#define VALUE_FROM_I64(_d_)    (value_t){.type = VALUE_INT64,  .i64 = _d_}
#define VALUE_FROM_FLOAT(_d_)  (value_t){.type = VALUE_FLOAT,  .f   = _d_}
#define VALUE_FROM_DOUBLE(_d_) (value_t){.type = VALUE_DOUBLE, .r   = _d_}

#define VALUE_EMPTY   (value_t){.type = VALUE_UNSET}

extern void value_add(value_t * dst, value_t *a, value_t* b);
extern void value_sub(value_t * dst, value_t *a, value_t* b);
extern void value_mult(value_t * dst, value_t *a, value_t* b);
extern void value_div(value_t * dst, value_t *a, value_t* b);

extern bool value_equ(value_t *a, value_t* b);
extern bool value_grt(value_t *a, value_t* b);
extern bool value_lst(value_t *a, value_t* b);

extern void value_log_debug(uint32_t flag, const char * prefix, value_t * v);
