#include <stdio.h>

#include "io.h"
#include "linux.h"


#define IO_COUNT                        16
#define IO_NAME_MAX_LEN                 16


typedef struct
{
    uint8_t     num;
    uint32_t    fd;
} fake_io_t;


static fake_io_t fake_ios[IO_COUNT];


extern bool     ios_get_pupd(unsigned io, uint8_t* pupd);


void     ios_init(void)
{
    for (uint8_t i = 0; i < IO_COUNT; i++)
    {
        fake_io_t* io = &fake_ios[i];
        io->num = i;
        char name[IO_NAME_MAX_LEN];
        snprintf(name, IO_NAME_MAX_LEN-1, "GPIO%02u", i);
        linux_add_gpio(name, &io->fd, NULL);
    }
}


unsigned ios_get_count(void)
{
    return IO_COUNT;
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
