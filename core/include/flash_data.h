#pragma once
#include <libopencm3/stm32/flash.h>

static inline void flash_set_data(const void * addr, const void * data, unsigned size)
{
    uintptr_t _addr = (uintptr_t)addr;

    unsigned left_over = size % 8;
    unsigned easy_size = size - left_over;

    if (easy_size)
        flash_program(_addr, (void*)data, easy_size);

    if (size > easy_size)
    {
        const uint8_t * p = (const uint8_t*)data;

        uint64_t v = 0;

        for(unsigned n = 0; n < left_over; n+=1)
        {
             v |= (p[easy_size + n]) << (8*n);
        }

        flash_program_double_word(_addr + easy_size, v);
    }
}


