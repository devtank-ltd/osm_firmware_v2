#include <string.h>

#include <esp_wifi.h>
#include <esp_event.h>

#include "log.h"
#include "common.h"
#include "base_types.h"
#include "persist_config.h"

#include "protocol.h"

#define SSID_LEN 16
#define WFPW_LEN 24
#define SVR_LEN 16
#define SVRUSR_LEN 16
#define SVRPW_LEN 24

#define MQTT_DEFAULT_PORT 1883

static volatile bool _has_ip_addr = false;
static volatile bool _has_mqtt = false;

typedef struct
{
    char ssid[SSID_LEN];
    char password[WFPW_LEN];
    char server[SVR_LEN];
    char svr_user[SVRUSR_LEN];
    char svr_pw[SVRPW_LEN];
    uint16_t svr_port;
    uint16_t authmode;
} __attribute__((__packed__)) osm_wifi_config_t;


static struct
{
    const char * name;
    wifi_auth_mode_t authmode;
} _authmodes[] =
{
    {"open", WIFI_AUTH_OPEN},
    {"WEP", WIFI_AUTH_WEP},
    {"WPA_PSK", WIFI_AUTH_WPA_PSK},
    {"WPA2_PSK", WIFI_AUTH_WPA2_PSK},
    {"WPA_WPA2_PSK", WIFI_AUTH_WPA_WPA2_PSK},
    {"WPA2_ENTERPRISE", WIFI_AUTH_WPA2_ENTERPRISE},
    {"WPA3_PSK", WIFI_AUTH_WPA3_PSK},
    {"WPA2_WPA3_PSK", WIFI_AUTH_WPA2_WPA3_PSK},
    {"WAPI_PSK", WIFI_AUTH_WAPI_PSK},
    {"OWE", WIFI_AUTH_OWE},
};


static osm_wifi_config_t* _wifi_get_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != COMMS_TYPE_WIFI)
    {
        comms_debug("Tried to get config for WiFi but config is not for WiFi.");
        return NULL;
    }
    return (osm_wifi_config_t*)(comms_config->setup);
}


static void _wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START:
                comms_debug("WiFi STA start event.");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                comms_debug("WiFi STA disconnect event.");
                _has_ip_addr = false;
                esp_wifi_connect();
                break;
            default:
                comms_debug("Unknown WiFi STA event :");
                break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                comms_debug("Got IP:"IPSTR, IP2STR(&event->ip_info.ip));
                _has_ip_addr = true;
                break;
            default:
                comms_debug("Unknown WiFi IP event :");
                break;
        }
    }
}


static void _start_wifi(void)
{
    osm_wifi_config_t* osm_config = _wifi_get_config();
    if (!osm_config)
        return;
    unsigned ssid_len   = strlen(osm_config->ssid);
    unsigned passwd_len = strlen(osm_config->password);

    if (!ssid_len || !passwd_len)
        return;

    wifi_config_t wifi_config =
    {
        .sta =
        {
            .threshold.authmode = osm_config->authmode,
            .pmf_cfg =
            {
                .capable = true,
                .required = false
            },
        },
    };

    memcpy(wifi_config.sta.ssid, osm_config->ssid, ssid_len+1);
    memcpy(wifi_config.sta.password, osm_config->password, passwd_len+1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

    ESP_ERROR_CHECK(esp_wifi_start());
}


void protocol_system_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, _wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, _wifi_event_handler, NULL));

    _start_wifi();
}


bool protocol_init(void) { return false; }

bool        protocol_append_measurement(measurements_def_t* def, measurements_data_t* data) { return false; }
bool        protocol_append_instant_measurement(measurements_def_t* def, measurements_reading_t* reading, measurements_value_type_t type) { return false; }
void        protocol_debug(void) {}
void        protocol_send(void) {}
bool        protocol_send_ready(void) { return false; }
bool        protocol_send_allowed(void) { return false; }
void        protocol_reset(void) {}
void        protocol_process(char* message) {}


bool protocol_get_connected(void)
{
    return _has_ip_addr;
}


void protocol_loop_iteration(void) {}

bool        protocol_get_id(char* str, uint8_t len) { return false; }

void        protocol_power_down(void) {}


static void _prep_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != COMMS_TYPE_WIFI)
    {
        memset(comms_config, 0, sizeof(comms_config_t));
        comms_config->type = COMMS_TYPE_WIFI;
    }
}


static command_response_t _get_set_str(unsigned dst_offset, unsigned dst_len, char * src, char * name, bool show)
{
    char * p = skip_space(src);
    if (!(*p))
    {
        osm_wifi_config_t* osm_config = _wifi_get_config();
        p = (osm_config)?((char*)osm_config) + dst_offset:"";
    }
    else
    {
        _prep_config();
        osm_wifi_config_t* osm_config = _wifi_get_config();
        unsigned len = strlen(p) + 1;
        if (len > dst_len)
        {
            log_out("Too long.");
            return COMMAND_RESP_ERR;
        }
        memcpy(((char*)osm_config) + dst_offset, p, len);
    }
    if (show)
        log_out("%s: %s", name, p);
    else if (*p)
        log_out("%s: is set", name);
    else
        log_out("%s: not set", name);

    return COMMAND_RESP_OK;
}


static command_response_t _ssid_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, ssid), SSID_LEN, args, "SSID", true);
}


static command_response_t _pw_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, password), WFPW_LEN, args, "Password", false);
}


static command_response_t _am_cb(char *args)
{
    char * p = skip_space(args);
    if (!(*p))
    {
        osm_wifi_config_t* osm_config = _wifi_get_config();
        wifi_auth_mode_t authmode = (osm_config)?osm_config->authmode:WIFI_AUTH_OPEN;
        for(unsigned n=0; n < ARRAY_SIZE(_authmodes); n++)
        {
            if (_authmodes[n].authmode == authmode)
            {
                log_out("authmode: %s", _authmodes[n].name);
                return COMMAND_RESP_OK;
            }
        }
        return COMMAND_RESP_ERR;
    }
    else
    {
        _prep_config();
        osm_wifi_config_t* osm_config = _wifi_get_config();
 
        char * am = p;
        for(unsigned n=0; n < ARRAY_SIZE(_authmodes); n++)
        {
            if (!strcmp(_authmodes[n].name, am))
            {
                log_out("authmode: %s", am);
                osm_config->authmode = _authmodes[n].authmode;
                return COMMAND_RESP_OK;
            }
        }
        log_out("Unknown authmode: %s", am);
        return COMMAND_RESP_ERR;
 
    }
    return COMMAND_RESP_OK;
}


static command_response_t _mqtt_svr_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, server), SVR_LEN, args, "MQTT Server", true);
}


static command_response_t _mqtt_usr_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr_user), SVRUSR_LEN, args, "MQTT User", true);
}


static command_response_t _mqtt_pw_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr_pw), SVRPW_LEN, args, "MQTT Password", false);
}



struct cmd_link_t* protocol_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "wifi_ssid", "Get/Set WiFi SSID",      _ssid_cb             , false , NULL },
        { "wifi_pw",   "Get/Set WiFi Password.", _pw_cb                      , false , NULL },
        { "wifi_am",   "Get/Set WiFi Auth Mode", _am_cb                       , false , NULL },
        { "mqtt_svr",   "Get/Set MQTT server", _mqtt_svr_cb                       , false , NULL },
        { "mqtt_usr",   "Get/Set MQTT user", _mqtt_usr_cb                       , false , NULL },
        { "mqtt_pw",   "Get/Set MQTT password", _mqtt_pw_cb                       , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}



