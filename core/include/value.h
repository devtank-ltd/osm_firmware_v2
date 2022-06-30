#pragma once
#include <stdint.h>
#include <stdbool.h>


#define VALUE_TYPE_IS_SIGNED    0x10
#define VALUE_STR_LEN           8

typedef enum
{
    VALUE_UNSET   = 0,
    VALUE_UINT8   = 1,
    VALUE_UINT16  = 2,
    VALUE_UINT32  = 3,
    VALUE_UINT64  = 4,
    VALUE_INT8    = 1 | VALUE_TYPE_IS_SIGNED,
    VALUE_INT16   = 2 | VALUE_TYPE_IS_SIGNED,
    VALUE_INT32   = 3 | VALUE_TYPE_IS_SIGNED,
    VALUE_INT64   = 4 | VALUE_TYPE_IS_SIGNED,
    VALUE_FLOAT   = 5 | VALUE_TYPE_IS_SIGNED,
    VALUE_STR     = 0x20,
} value_type_t;

typedef struct
{
    uint8_t type;
    union
    {
        uint8_t  u8;
        int8_t   i8;
        uint16_t u16;
        int16_t  i16;
        uint32_t u32;
        int32_t  i32;
        uint64_t u64;
        int64_t  i64;
        float    f;
        char     str[VALUE_STR_LEN];
    };
} value_t;

static inline value_t value_from_u8(uint8_t d)    { return (value_t){.type = VALUE_UINT8,  .u8  = d}; }
static inline value_t value_from_i8(int8_t d)     { return (value_t){.type = VALUE_INT8,   .i8  = d}; }
static inline value_t value_from_u16(uint16_t d)  { return (value_t){.type = VALUE_UINT16, .u16 = d}; }
static inline value_t value_from_i16(int16_t d)   { return (value_t){.type = VALUE_INT16,  .i16 = d}; }
static inline value_t value_from_u32(uint32_t d)  { return (value_t){.type = VALUE_UINT32, .u32 = d}; }
static inline value_t value_from_i32(int32_t d)   { return (value_t){.type = VALUE_INT32,  .i32 = d}; }
static inline value_t value_from_u64(uint64_t d)  { return (value_t){.type = VALUE_UINT64, .u64 = d}; }
static inline value_t value_from_i64(int64_t d)   { return (value_t){.type = VALUE_INT64,  .i64 = d}; }
static inline value_t value_from_float(float d)   { return (value_t){.type = VALUE_FLOAT,  .f   = d}; }


extern bool value_as_str(value_t* value, char* d, unsigned len);


#define value_from(_d_) _Generic((_d_),                                   \
                                    unsigned char:      value_from_u8,    \
                                    char:               value_from_i8,    \
                                    unsigned short :    value_from_u16,   \
                                    short :             value_from_i16,   \
                                    unsigned long:      value_from_u32,   \
                                    long:               value_from_i32,   \
                                    unsigned long long: value_from_u64,   \
                                    long long:          value_from_i64,   \
                                    float :             value_from_float  \
                                    )(_d_)

#define value_get(_v_) ( (_v_->type == VALUE_UINT8)?_v_->u8:  \
                         (_v_->type == VALUE_INT8)?_v_->i8:   \
                         (_v_->type == VALUE_UINT16)?_v_->u16:\
                         (_v_->type == VALUE_INT16)?_v_->i16: \
                         (_v_->type == VALUE_UINT32)?_v_->u32:\
                         (_v_->type == VALUE_INT32)?_v_->i32: \
                         (_v_->type == VALUE_UINT64)?_v_->u64:\
                         (_v_->type == VALUE_INT64)?_v_->i64: \
                         (_v_->type == VALUE_FLOAT)?_v_->f:0 )


#define VALUE_EMPTY   (value_t){.type = VALUE_UNSET}

extern void value_compact(value_t * v);

extern bool value_add(value_t * dst, value_t *a, value_t* b);
extern bool value_sub(value_t * dst, value_t *a, value_t* b);
extern bool value_mult(value_t * dst, value_t *a, value_t* b);
extern bool value_div(value_t * dst, value_t *a, value_t* b);

extern bool value_equ(value_t *a, value_t* b);
extern bool value_grt(value_t *a, value_t* b);
extern bool value_lst(value_t *a, value_t* b);

extern bool value_from_str(value_t * v, char * str);

extern bool value_to_str(value_t * v, char * str, unsigned size);
