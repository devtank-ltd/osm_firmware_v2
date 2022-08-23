#include "comms.h"


#define COMMS_DEFAULT_MTU       256


uint16_t comms_get_mtu(void)
{
    return COMMS_DEFAULT_MTU;
}


bool comms_send_ready(void)
{
    return true;
}


bool comms_send_str(char* str)
{
    return true;
}


bool comms_send_allowed(void)
{
    return true;
}


void comms_send(int8_t* hex_arr, uint16_t arr_len)
{
}

void comms_init(void)
{
}


void comms_reset(void)
{
}


void comms_process(char* message)
{
}


bool comms_get_connected(void)
{
    return false;
}


void comms_loop_iteration(void)
{
}


void comms_config_setup_str(char * str)
{
}
