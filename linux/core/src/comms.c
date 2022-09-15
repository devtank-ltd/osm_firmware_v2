#include <inttypes.h>
#include <string.h>
#include <stdio.h>


#include "comms.h"
#include "config.h"
#include "uart_rings.h"


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
    if(!uart_ring_out(LW_UART, str, strlen(str)))
        return false;
    return uart_ring_out(LW_UART, "\r\n", 2);
}


bool comms_send_allowed(void)
{
    return true;
}


void comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    char buf[3];
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(buf, 3, "%.2"PRIu32, hex_arr[i]);
        uart_ring_out(LW_UART, buf, 2);
    }
    uart_ring_out(LW_UART, "\r\n", 2);
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


bool comms_get_id(char* str, uint8_t len)
{
    return false;
}
