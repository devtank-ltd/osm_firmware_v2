#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>


#include "uart_rings.h"
#include "measurements.h"
#include "platform.h"


// Timing Functions

uint32_t since_boot_delta(uint32_t newer, uint32_t older)
{
    if (newer < older)
        return (0xFFFFFFFF - older) + newer;
    else
        return newer - older;
}


void spin_blocking_ms(uint32_t ms)
{
    uint64_t num_loops = (rcc_ahb_frequency / 1e4) * ms;
    for (uint64_t i = 0; i < num_loops; i++)
    {
        asm("nop");
    }
}


// Maths Functions


int32_t nlz(uint32_t x)
{
    int32_t y, m, n;

    y = -(x >> 16);
    m = (y >> 16) & 16;
    n = 16 - m;
    x = x >> m;

    y = x - 0x100;
    m = (y >> 16) & 8;
    n = n + m;
    x = x << m;

    y = x - 0x1000;
    m = (y >> 16) & 4;
    n = n + m;
    x = x << m;

    y = x - 0x4000;
    m = (y >> 16) & 2;
    n = n + m;
    x = x << m;

    y = x >> 14;
    m = y & ~(y >> 1);
    return n + 2 - m;
}


int32_t ilog10(uint32_t x)
{
    int32_t y;
    static uint32_t table2[11] = {0, 9, 99, 999, 9999, 99999, 999999, 9999999, 99999999, 999999999, 0xFFFFFFFF};
    y = (19 * (31 - nlz(x))) >> 6;
    y = y + ((table2[y+1] - x) >> 31);
    return y;
}


uint64_t abs_i64(int64_t val)
{
    if (val < 0)
        return -val;
    return val;
}


bool u64_multiply_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2)
{
    if (arg_1 >= UINT64_MAX / arg_2)
    {
        return false;
    }
    *result = arg_1 * arg_2;
    return true;
}


bool u64_addition_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2)
{
    if (arg_1 >= UINT64_MAX - arg_2)
    {
        return false;
    }
    *result = arg_1 + arg_2;
    return true;
}
