#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "basic_fixed.h"


#define BFP_BIG_ONE    10000000000000000000ULL
#define BFP_BIG_ONE_MUL 10000000UL

#define STR_EXPAND(tok) #tok            ///< Convert macro value to a string.
#define STR(tok) STR_EXPAND(tok)        ///< Convert macro value to a string.


static int64_t basic_fixed_get(basic_fixed_t * v)
{
    int sign = (v->sign)?-1:1;
    return (((int64_t)v->upper) * BFP_LOWEROVER + ((int64_t)v->lower)) * sign;
}


static void basic_fixed_set(basic_fixed_t * v, int64_t value)
{
    v->sign = (value < 0)?1:0;
    uint64_t abs = llabs(value);
    v->upper = abs / BFP_LOWEROVER;
    v->lower = abs % BFP_LOWEROVER;
}


int basic_fixed_to_str(basic_fixed_t * v, char * buffer, unsigned size)
{
    if (!v || !buffer || !size)
        return -1;

    int64_t major = v->upper;
    unsigned minor = v->lower;

    return snprintf(buffer, size, "%s%"PRId64".%0"STR(BFP_LOWERLEN)"u", (v->sign && (major || minor))?"-":"", major, minor);
}


bool basic_fixed_read(char * str, basic_fixed_t * r, char ** endpos)
{
    if (!str || !r)
        return false;

    if (*str == '-')
    {
        r->sign = 1;
        str++;
    }
    else r->sign = 0;

    char * frac = NULL;
    uint64_t upper = strtoull(str, &frac, 10);

    if (upper > BFP_UPPERMAX)
        return false;

    r->upper = llabs(upper);

    if (*frac != '.')
    {
        if (endpos)
            *endpos = frac;
        r->lower = 0;
        return true;
    }

    frac++;

    unsigned len = strlen(frac);
    unsigned lower = strtoul(frac, endpos, 10);

    while(len > BFP_LOWERLEN)
    {
        lower /= 10;
        len--;
    }

    while(len < BFP_LOWERLEN)
    {
        lower *= 10;
        len++;
    }

    r->lower = lower;

    return true;
}


bool basic_fixed_set_whole(basic_fixed_t * v, int64_t value)
{
    if (value > BFP_UPPERMAX || value < -BFP_UPPERMAX)
        return false;

    v->sign = (value < 0)?1:0;
    v->upper = llabs(value);
    v->lower = 0;

    return true;
}


bool basic_fixed_add(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b)
{
    if (!v || !a || !b)
        return false;

    int64_t v_a = basic_fixed_get(a);
    int64_t v_b = basic_fixed_get(b);
    int64_t v_c = v_a + v_b;

    basic_fixed_set(v, v_c);
    return true;
}


bool basic_fixed_sub(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b)
{
    if (!v || !a || !b)
        return false;

    int64_t v_a = basic_fixed_get(a);
    int64_t v_b = basic_fixed_get(b);
    int64_t v_c = v_a - v_b;

    basic_fixed_set(v, v_c);
    return true;
}


bool basic_fixed_mul(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b)
{
    if (!v || !a || !b)
        return false;

    uint64_t a_upper = a->upper;
    uint64_t b_upper = b->upper;
    uint64_t a_lower = a->lower;
    uint64_t b_lower = b->lower;

    uint64_t c_upper = a_upper * b_upper;
    uint64_t c_frac = (a_upper * b_lower) + (b_upper * a_lower) + (a_lower * b_lower / BFP_LOWEROVER);

    c_upper += c_frac / BFP_LOWEROVER;
    c_frac %= BFP_LOWEROVER;

    v->upper = c_upper;
    v->lower = c_frac;
    v->sign = (a->sign || b->sign);

    return true;
}


bool basic_fixed_div(basic_fixed_t * v, basic_fixed_t * a,  basic_fixed_t * b)
{
    if (!v || !a || !b)
        return false;

    if (!b->upper && !b->lower)
        return false;

    if (!a->lower && !b->lower && a->upper && b->upper)
    {
        uint64_t a_upper = a->upper * BFP_LOWEROVER;
        uint64_t result =  a_upper / b->upper;
        v->upper = result / BFP_LOWEROVER;
        v->lower = result % BFP_LOWEROVER;
    }
    else if (!a->upper && !b->upper && a->lower && a->lower)
    {
        uint64_t a_lower = a->lower * BFP_LOWEROVER;
        uint64_t result =  a_lower / b->lower;
        v->upper = result / BFP_LOWEROVER;
        v->lower = result % BFP_LOWEROVER;
    }
    else if (!b->upper && b->lower)
    {
        uint64_t whole = a->upper * BFP_LOWEROVER + a->lower;
        uint64_t result =  whole / b->lower;
        v->upper = result;
        v->lower = 0;
    }
    else
    {
        uint64_t whole = (b->upper * BFP_LOWEROVER) + b->lower;
        uint64_t big_reciprocal = BFP_BIG_ONE / whole;
        uint64_t big_reci_frac = big_reciprocal / BFP_BIG_ONE_MUL;

        basic_fixed_t bf_reci = {.sign  = 0,
                                 .upper = big_reci_frac / BFP_LOWEROVER,
                                 .lower = big_reci_frac % BFP_LOWEROVER,
                                };

        if (!basic_fixed_mul(v, a, &bf_reci))
            return false;
    }

    v->sign = (a->sign ^ b->sign);
    return true;
}
