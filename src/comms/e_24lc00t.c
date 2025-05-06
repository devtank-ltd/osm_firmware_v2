#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/i2c.h>
#include "model_config.h"
#include <osm/core/log.h>

#define E_24LC00T_ADDR_MASK             0x78
#define E_24LC00T_ADDR                  0x50 /* Last 3bits of address ignored*/

#define E_24LC00T_TIMEOUT_MS            10
#define E_24LC00T_MAX_FAIL_COUNT        10


static bool _e_24lc00t_sequential_read(uint8_t start_addr, uint8_t* mem, unsigned memlen)
{
    return i2c_transfer_timeout(E_24LC00T_I2C, E_24LC00T_ADDR, &start_addr, 1, mem, memlen, E_24LC00T_TIMEOUT_MS);
}


static bool _e_24lc00t_write(uint8_t addr, uint8_t value)
{
    uint8_t packet[2];
    packet[0] = addr;
    packet[1] = value;
    return i2c_transfer_timeout(E_24LC00T_I2C, E_24LC00T_ADDR, packet, 2, NULL, 0, E_24LC00T_TIMEOUT_MS);
}


bool e_24lc00t_read(void* mem, unsigned len)
{
    return _e_24lc00t_sequential_read(0, mem, len);
}


bool e_24lc00t_write(void* mem, unsigned len)
{
    for (unsigned i = 0; i < len; i++)
    {
        unsigned fail_count = 0;
        for (;E_24LC00T_MAX_FAIL_COUNT >= fail_count; fail_count++)
        {
            if (_e_24lc00t_write(i, *((uint8_t*)mem + i)))
            {
                break;
            }
        }
        if (E_24LC00T_MAX_FAIL_COUNT <= fail_count)
        {
            return false;
        }
    }
    return true;
}
