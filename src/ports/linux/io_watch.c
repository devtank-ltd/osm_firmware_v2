#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "io.h"
#include "io_watch.h"
#include "config.h"
#include "platform.h"
#include "log.h"


const unsigned              ios_watch_ios[IOS_WATCH_COUNT]                  = IOS_WATCH_IOS;
static measurements_def_t*  _ios_watch_measurements_def[IOS_WATCH_COUNT];
static measurements_data_t* _ios_watch_measurements_data[IOS_WATCH_COUNT];


bool io_watch_enable(unsigned io, bool enabled, io_pupd_t pupd)
{
    if (enabled)
    {
        /* Start listening to fd or something? */
        return true;
    }
    /* Remove interrupt etc. */
    return true;
}


void io_watch_isr(uint32_t exti_group)
{
    for (unsigned i = 0; i < IOS_WATCH_COUNT; i++)
    {

        measurements_def_t* def = _ios_watch_measurements_def[i];
        measurements_data_t* data = _ios_watch_measurements_data[i];
        if (!def || !data)
            return;

        if (!def->interval || !def->samplecount)
            return;

        data->instant_send = 1;
    }
}


void io_watch_init(void)
{
    /* Must be called after measurements_init */
    char name[MEASURE_NAME_NULLED_LEN] = IOS_MEASUREMENT_NAME_PRE;
    unsigned len = strnlen(name, MEASURE_NAME_LEN);
    char* p = name + len;
    for (unsigned i = 0; i < IOS_WATCH_COUNT; i++)
    {
        snprintf(p, MEASURE_NAME_NULLED_LEN - len, "%02u", ios_watch_ios[i]);
        if (!measurements_get_measurements_def(name, &_ios_watch_measurements_def[i], &_ios_watch_measurements_data[i]))
        {
            _ios_watch_measurements_def[i] = NULL;
            _ios_watch_measurements_data[i] = NULL;
            io_debug("Could not find measurements data for '%s'", name);
        }
    }
}
