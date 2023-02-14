#pragma once
#include <stdbool.h>

#include "base_types.h"
#include "io.h"


#define IOS_WATCH_COUNT                     2


extern const unsigned ios_watch_ios[IOS_WATCH_COUNT];


bool io_watch_enable(unsigned io, bool enabled, io_pupd_t pupd);
void io_watch_handle_interrupt(unsigned io);
void io_watch_init(void);
