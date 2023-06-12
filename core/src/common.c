#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>


#include "config.h"
#include "common.h"

#include "uart_rings.h"
#include "measurements.h"
#include "platform.h"
#include "config.h"
#include "log.h"
#include "pinmap.h"


bool msg_is(const char* ref, char* message)
{
    return (bool)(strncmp(message, ref, strlen(ref)) == 0);
}


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
    static const uint32_t table2[11] = {0, 9, 99, 999, 9999, 99999, 999999, 9999999, 99999999, 999999999, 0xFFFFFFFF};
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


bool main_loop_iterate_for(uint32_t timeout, bool (*should_exit_db)(void *userdata),  void *userdata)
{
    uint32_t start_time = get_since_boot_ms();
    uint32_t watch_dog_kick = IWDG_NORMAL_TIME_MS/2;

    platform_watchdog_reset();
    if (timeout > IWDG_NORMAL_TIME_MS)
        log_debug(DEBUG_SYS, "Warning, timeout required watchdog kicking.");

    if (should_exit_db(userdata))
        return true;

    while(since_boot_delta(get_since_boot_ms(), start_time) < timeout)
    {
        for(unsigned uart = 0; uart < UART_CHANNELS_COUNT; uart++)
        {
            if (uart != CMD_UART)
                uart_ring_in_drain(uart);
        }

        uart_rings_out_drain();
        platform_tight_loop();
        if (should_exit_db(userdata))
            return true;

        if (since_boot_delta(get_since_boot_ms(), start_time) > watch_dog_kick)
        {
            log_debug(DEBUG_SYS, "Kicking watchdog.");
            platform_watchdog_reset();
            watch_dog_kick += (IWDG_NORMAL_TIME_MS/2);
        }
    }
    return false;
}


int32_t to_f32_from_float(float in)
{
    return (int32_t)(in * 1000);
}


int32_t to_f32_from_double(double in)
{
    return (int32_t)(in * 1000);
}


struct cmd_link_t* add_commands(struct cmd_link_t* tail, struct cmd_link_t* cmds, unsigned num_cmds)
{
    if (!tail | !cmds)
    {
        log_error("Tail or command list is NULL");
        return tail;
    }
    for (unsigned i = 0; i < num_cmds; i++)
    {
        tail->next = &cmds[i];
        tail = tail->next;
    }
    return tail;
}


uint16_t modbus_crc(uint8_t * buf, unsigned length)
{
    uint16_t crc = 0xFFFF;

    for (unsigned pos = 0; pos < length; pos++)
    {
        crc ^= (uint16_t)buf[pos];        // XOR byte into least sig. byte of crc

        for (int i = 8; i != 0; i--)      // Loop over each bit
        {
            if ((crc & 0x0001) != 0)      // If the LSB is set
            {
                crc >>= 1;                // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else                          // Else LSB is not set
                crc >>= 1;                // Just shift right
        }
    }
    return crc;
}
