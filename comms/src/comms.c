#include "comms.h"

#include "lw.h"
#include "rak4270.h"
#include "rak3172.h"
#include "version.h"


uint16_t comms_get_mtu(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_get_mtu();
        case VERSION_ARCH_REV_C:
            return rak3172_get_mtu();
        default:
            return 0;
    }
}


bool comms_send_ready(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_send_ready();
        case VERSION_ARCH_REV_C:
            return rak3172_send_ready();
        default:
            return false;
    }
}


bool comms_send_str(char* str)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_send_str(str);
        case VERSION_ARCH_REV_C:
            return rak3172_send_ready();
        default:
            return false;
    }
}


bool comms_send_allowed(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_send_allowed();
        case VERSION_ARCH_REV_C:
            return rak3172_send_allowed();
        default:
            return false;
    }
}


void comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_send(hex_arr, arr_len);
            return;
        case VERSION_ARCH_REV_C:
            rak3172_send(hex_arr, arr_len);
            return;
        default:
            return;
    }
}


void comms_init(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_init();
            return;
        case VERSION_ARCH_REV_C:
            rak3172_init();
            return;
        default:
            return;
    }
}


void comms_reset(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_reset();
            return;
        case VERSION_ARCH_REV_C:
            rak3172_reset();
            return;
        default:
            return;
    }
}


void comms_process(char* message)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_process(message);
            return;
        case VERSION_ARCH_REV_C:
            rak3172_process(message);
            return;
        default:
            return;
    }
}


bool comms_get_connected(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_get_connected();
        case VERSION_ARCH_REV_C:
            return rak3172_get_connected();
        default:
            return false;
    }
}


void comms_loop_iteration(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_loop_iteration();
            return;
        case VERSION_ARCH_REV_C:
            rak3172_loop_iteration();
            return;
        default:
            return;
    }
}


void comms_config_setup_str(char * str)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_config_setup_str(str);
            return;
        case VERSION_ARCH_REV_C:
            rak3172_config_setup_str(str);
            return;
        default:
            return;
    }
}


bool comms_get_id(char* str, uint8_t len)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return lw_get_id(str, len);
        case VERSION_ARCH_REV_C:
            return lw_get_id(str, len);
        default:
            return false;
    }
}
