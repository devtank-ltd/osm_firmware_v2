#include <inttypes.h>
#include <stdbool.h>


bool rp2350_comms_send(char* data, uint16_t len)
{
    return false;
}


void rp2350_comms_init(void)
{
}


bool rp2350_comms_send_ready(void)
{
    return false;
}


bool rp2350_comms_send_allowed(void)
{
    return false;
}


void rp2350_comms_reset(void)
{
}


bool rp2350_comms_get_connected(void)
{
    return false;
}

void rp2350_comms_process(char* message)
{
}

void rp2350_comms_loop_iteration(void)
{
}

void rp2350_comms_power_down(void)
{
}


bool rp2350_comms_get_unix_time(int64_t * ts)
{
    return false;
}


bool rp2350_comms_send_str(char* str)
{
    return false;
}


bool rp2350_comms_get_id(char* str, uint8_t len)
{
    return false;
}


bool rp2350_comms_persist_config_cmp(void* d0, void* d1)
{
    return false;
}


struct cmd_link_t* comms_add_commands(struct cmd_link_t* tail)
{
    return tail;
}
