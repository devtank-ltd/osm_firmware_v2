#ifndef __BASIC_FIXED_POINT__
#define __BASIC_FIXED_POINT__

#include <stdint.h>
#include <stdbool.h>


typedef union
{
    struct
    {
        uint64_t lower:20;
        uint64_t upper:43;
        uint64_t sign;
    }__attribute__((packed));
    uint64_t raw;
} __attribute__((packed)) basic_fixed_t;


int basic_fixed_to_str(basic_fixed_t * v, char * buffer, unsigned size);

bool basic_fixed_read(char * str, basic_fixed_t * r, char ** endpos);

bool basic_fixed_add(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b);

bool basic_fixed_sub(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b);

bool basic_fixed_mul(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b);

bool basic_fixed_div(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b);

#endif //__BASIC_FIXED_POINT__
