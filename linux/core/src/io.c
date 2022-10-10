#include <stdio.h>

#include "io.h"
#include "linux.h"
#include "config.h"
#include "pinmap.h"


#define IO_NAME_MAX_LEN                 16


extern bool     ios_get_pupd(unsigned io, uint8_t* pupd);


void     ios_init(void)
{
}


unsigned ios_get_count(void)
{
    return IOS_COUNT;
}


void     io_configure(unsigned io, bool as_input, io_pupd_t pull)
{
}


bool     io_enable_pulsecount(unsigned io, io_pupd_t pupd)
{
    return true;
}


bool     io_enable_w1(unsigned io)
{
    // TO DO: connect/disconnect domain socket of w1 ICP.
    return true;
}


bool     io_is_pulsecount_now(unsigned io)
{
    return false;
}


bool     io_is_w1_now(unsigned io)
{
    return false;
}


bool     io_is_input(unsigned io)
{
    return false;
}


unsigned io_get_bias(unsigned io)
{
    return 0;
}


void     io_on(unsigned io, bool on_off)
{
}


void     io_log(unsigned io)
{
}


void     ios_log(void)
{
}
