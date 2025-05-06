#include <stdbool.h>
#include <inttypes.h>

#include <osm/core/w1.h>

#include <osm/core/timers.h>
#include "pinmap.h"
#include <osm/core/log.h>
#include <osm/core/base_types.h>
#include <osm/core/io.h>
#include "platform_model.h"


bool w1_reset(uint8_t index)
{
    return false;
}


uint8_t w1_read_byte(uint8_t index)
{
    return 0;
}


void w1_send_byte(uint8_t index, uint8_t byte)
{
}


void w1_init(uint8_t index)
{
}


void w1_enable(unsigned io, bool enabled)
{
}
