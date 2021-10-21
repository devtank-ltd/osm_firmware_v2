#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "uart_rings.h"
#include "uarts.h"
#include "config.h"
#include "lorawan.h"
#include "log.h"


#define LW_BUFFER_SIZE 64

#define LW_JOIN_MODE_OTAA   0
#define LW_JOIN_MODE_ABP    1
#define LW_JOIN_MODE        LW_JOIN_MODE_OTAA
#define LW_CLASS_A          0
#define LW_CLASS_C          2
#define LW_CLASS            LW_CLASS_A
#define LW_REGION           "EU868"
// TODO: Dev EUI and App Key should not be baked in.
#define LW_DEV_EUI          "118f875d6994bbfd"
// For Chirpstack OTAA the App EUI must match the Dev EUI
#define LW_APP_EUI          LW_DEV_EUI
#define LW_APP_KEY          "d9597152b1293bb9c0e220cd04fc973c"


static char lw_buffer[LW_BUFFER_SIZE] = {0};


static void lw_write_to_uart(char* cmd_fmt, ...)
{
    va_list args;
    size_t len;

    va_start(args, cmd_fmt);
    vsnprintf(lw_buffer, LW_BUFFER_SIZE, cmd_fmt, args);
    va_end(args);

    len = strlen(lw_buffer);
    lw_buffer[len] = '\r';
    lw_buffer[len+1] = '\n';
    log_sys_debug("LoRaWAN out to %u", LW_UART);
    uart_ring_out(LW_UART, lw_buffer, LW_BUFFER_SIZE);
    log_out(lw_buffer);
    memset(lw_buffer, 0, LW_BUFFER_SIZE);
}


static void lw_set_confirmation(bool conf)
{
    uint8_t val = 0;
    if (conf)
    {
        val = 1;
    }
    lw_write_to_uart("at+set_config=lora:confirm:%u", val);
}

/*
static void lw_set_parameters(void)
{
    lw_write_to_uart("at+set_config=lora:default_parameters");
    lw_write_to_uart("at+set_config=lora:join_mode:%u", LW_JOIN_MODE);
    lw_write_to_uart("at+set_config=lora:class:%u", LW_CLASS);
    lw_write_to_uart("at+set_config=lora:region:%s", LW_REGION);
    lw_write_to_uart("at+set_config=lora:dev_eui:%s", LW_DEV_EUI);
    lw_write_to_uart("at+set_config=lora:app_eui:%s", LW_APP_EUI);
    lw_write_to_uart("at+set_config=lora:app_key:%s", LW_APP_KEY);
    lw_write_to_uart("at+set_config=device:restart");
}*/


void lorawan_init(void)
{
    lw_write_to_uart("at+join");
}


static bool lw_payload_add(lw_measurement_t* m, uint64_t* payload)
{
    uint64_t p;
    p = m->channel << (2 * 4);
    p = p + m->type_id;
    p = (p << (m->data_size * 8)) + m->data;
    *payload = (*payload << ((m->data_size*2 + 4) * 4)) + p;
    return true;
}


bool lw_send_packet(lw_packet_t* packet)
{
    uint64_t payload = 0;
    uint8_t port = 5;
    lw_set_confirmation(packet->confirm);
    for(lw_measurement_t* m = packet->measurements; m->id; m++)
    {
        if (!lw_payload_add(m, &payload))
        {
            return false;
        }
    }
    lw_write_to_uart("at+send=lora:%u:%llX", port, payload);
    return true;
}


void lw_main(void)
{/*
    lw_packet_t packet;
    packet.confirm = true;
    lw_measurement_t measurements[] =
    {
        { 1, 3, LW_ID__TEMPERATURE, 2 , 272},
        { 2, 5, LW_ID__TEMPERATURE, 2 , 255},
    };
    *packet.measurements = *measurements;
    lw_send_packet(&packet);*/
}
