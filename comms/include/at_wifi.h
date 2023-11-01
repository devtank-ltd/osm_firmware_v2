#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include "base_types.h"


#define AT_WIFI_MAX_SSID_LEN                    63
#define AT_WIFI_MAX_PWD_LEN                     63
#define AT_WIFI_MQTT_ADDR_MAX_LEN               63
#define AT_WIFI_MQTT_USER_MAX_LEN               63
#define AT_WIFI_MQTT_PWD_MAX_LEN                63
#define AT_WIFI_MQTT_CA_MAX_LEN                 63


typedef struct
{
    uint8_t     type;
    uint8_t     _[3];
    struct
    {
        char        ssid[AT_WIFI_MAX_SSID_LEN + 1];
        char        pwd[AT_WIFI_MAX_PWD_LEN + 1];
    } wifi;
    struct
    {
        char        addr[AT_WIFI_MQTT_ADDR_MAX_LEN + 1];
        char        user[AT_WIFI_MQTT_USER_MAX_LEN + 1];
        char        pwd[AT_WIFI_MQTT_PWD_MAX_LEN + 1];
        char        ca[AT_WIFI_MQTT_CA_MAX_LEN + 1];
        uint16_t    port;
    } mqtt;
} __attribute__((__packed__)) at_wifi_config_t;

_Static_assert(sizeof(at_wifi_config_t) < sizeof(comms_config_t), "LoRaWAN config too big.");


extern uint16_t at_wifi_get_mtu(void);

extern bool     at_wifi_send_ready(void);
extern bool     at_wifi_send_str(char* str);
extern bool     at_wifi_send_allowed(void);
extern void     at_wifi_send(char* data, uint16_t len);

extern void     at_wifi_init(void);
extern void     at_wifi_reset(void);
extern void     at_wifi_process(char* message);
extern bool     at_wifi_get_connected(void);
extern void     at_wifi_loop_iteration(void);

extern bool     at_wifi_get_id(char* str, uint8_t len);

extern struct cmd_link_t* at_wifi_add_commands(struct cmd_link_t* tail);

extern void     at_wifi_power_down(void);

extern bool     at_wifi_persist_config_cmp(void* d0, void* d1);
