#include <ctype.h>
#include <sys/time.h>

#include <esp_wifi.h>
#include <esp_event.h>

#include <mqtt_client.h>

#include "log.h"
#include "cmd.h"
#include "common.h"
#include "base_types.h"
#include "persist_config.h"
#include "mqtt.h"
#include "esp_comms.h"
#include "protocol.h"
#include "uart_rings.h"
#include "platform_model.h"

#define WIFI_DELAY_MS 1000

#define SSID_LEN   16
#define WFPW_LEN   32
#define SVR_LEN    24
#define SVRUSR_LEN 12
#define SVRPW_LEN  32

static char _mac_str[16];

static volatile bool _has_ip_addr = false;
static volatile bool _has_mqtt = false;
static volatile int _qos = 1;

static esp_mqtt_client_handle_t _client = NULL;

static char _cmd[32];
static volatile bool _cmd_ready = false;

static bool _wifi_started = false;
static bool _mqtt_started = false;



typedef struct
{
    uint8_t  type;
    uint8_t  authmode;
    uint16_t autostart:1;
    uint16_t fwd_uart:1;
    uint16_t _:14;
    char ssid[SSID_LEN];
    char password[WFPW_LEN];
    char svr[SVR_LEN];
    char svr_user[SVRUSR_LEN];
    char svr_pw[SVRPW_LEN];
} __attribute__((__packed__)) osm_wifi_config_t;

_Static_assert(sizeof(osm_wifi_config_t) < sizeof(comms_config_t), "WiFi config too big.");


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


static bool _mqtt_send(const char * topic, const char * value, unsigned len)
{
    int msg_id = esp_mqtt_client_publish(_client, topic, value, len, _qos, false);
    if (msg_id < 0)
    {
        log_error("Fail to send MQTT \"%s\"", topic);
        return false;
    }
    else
    {
        comms_debug("Sent MQTT \"%s\" (%d)", topic, msg_id);
        return true;
    }
}


bool esp_comms_send(char * data, uint16_t len)
{
    char topic[64];
    snprintf(topic, sizeof(topic), "osm/%s/measurements", _mac_str);
    topic[sizeof(topic)-1] = 0;
    bool r = _mqtt_send(topic, data, len);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return r;
}


static osm_wifi_config_t* _wifi_get_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != COMMS_TYPE_WIFI)
    {
        comms_debug("Tried to get config for WiFi but config is not for WiFi.");
        return NULL;
    }
    return (osm_wifi_config_t*)(comms_config);
}


void mqtt_uart_forward(const char * data, unsigned size)
{
    osm_wifi_config_t* osm_config = _wifi_get_config();
    if (!osm_config || !_has_mqtt || !osm_config->fwd_uart)
        return;
    char topic[64];
    snprintf(topic, sizeof(topic), "osm/%s/fwd", _mac_str);
    topic[sizeof(topic)-1] = 0;
    _mqtt_send(topic, data, size);
}


static void _mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            _has_mqtt = true;
            comms_debug("MQTT connected.");
            char topic[32];
            snprintf(topic, sizeof(topic), "osm/%s/cmd", _mac_str);
            topic[sizeof(topic)-1] = 0;
            esp_mqtt_client_subscribe(client, topic, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            _has_mqtt = false;
            comms_debug("MQTT disconnected.");
            break;
        case MQTT_EVENT_DATA:
            comms_debug("MQTT Data %.*s", event->topic_len, event->topic);
            if (_cmd_ready)
            {
                comms_debug("MQTT msg already pending.");
                break;
            }
            memset(_cmd, 0, sizeof(_cmd));
            if (event->data_len < sizeof(_cmd))
            {
                memcpy(_cmd, event->data, event->data_len);
                _cmd_ready = true;
            }
            else
                comms_debug("MQTT msg too long.");
            break;
        default:
            break;
    }
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
                _has_mqtt = false;
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
                const uint8_t * mac = esp_port_get_mac();
                snprintf(_mac_str, sizeof(_mac_str), "%"PRIx8"%"PRIx8"%"PRIx8"%"PRIx8"%"PRIx8"%"PRIx8, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                comms_debug("MAC: %s", _mac_str);

                _has_ip_addr = true;
                break;
            default:
                comms_debug("Unknown WiFi IP event :");
                break;
        }
    }
}


static void _mqtt_start(void)
{
    if (_mqtt_started || !_has_ip_addr)
        return;
    osm_wifi_config_t* osm_config = _wifi_get_config();
    static char uri[5 + SSID_LEN + WFPW_LEN + SVR_LEN + SVRUSR_LEN + SVRPW_LEN];

    //mqtt://username:password@mqtt.eclipseprojects.io
    comms_debug("Connecting to \"mqtt://%s@%s\"", osm_config->svr_user, osm_config->svr);
    snprintf(uri, sizeof(uri), "mqtt://%s:%s@%s", osm_config->svr_user, osm_config->svr_pw, osm_config->svr);
    uri[sizeof(uri)-1]=0;

    esp_mqtt_client_config_t mqtt_cfg =
    {
        .broker.address.uri = uri,
    };
    _client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(_client, ESP_EVENT_ANY_ID, _mqtt_event_handler, NULL);
    esp_mqtt_client_start(_client);
    _mqtt_started = true;
}


static void _wifi_start(void)
{
    if (_wifi_started)
        return;

    osm_wifi_config_t* osm_config = _wifi_get_config();
    if (!osm_config)
        return;
    unsigned ssid_len   = strlen(osm_config->ssid);
    unsigned passwd_len = strlen(osm_config->password);

    if (!ssid_len || !passwd_len)
        return;

    if ((ssid_len > (SSID_LEN - 1)) || (passwd_len > (WFPW_LEN - 1)))
        return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, _wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, _wifi_event_handler, NULL));

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
    _wifi_started = true;
}


void esp_comms_init(void)
{
}


bool        esp_comms_send_ready(void) { return _has_mqtt; }
bool        esp_comms_send_allowed(void) { return _has_mqtt; }
void        esp_comms_reset(void) {}


bool esp_comms_get_connected(void)
{
    return _has_mqtt;
}

void        esp_comms_process(char* message) {}

void esp_comms_loop_iteration(void)
{
    if (!_wifi_started)
    {
        osm_wifi_config_t* osm_config = _wifi_get_config();
        if (osm_config && osm_config->autostart)
        {
            /* Delay wifi start up. */
            uint32_t now = get_since_boot_ms();
            if (now > WIFI_DELAY_MS)
                _wifi_start();
        }
    }
    else if (!_mqtt_started)
        _mqtt_start();

    if (!_cmd_ready)
        return;
    command_response_t resp = cmds_process(_cmd, strlen(_cmd), NULL);
    char topic[64];
    snprintf(topic, sizeof(topic), "/osm/%s/cmd/resp", _mac_str);
    topic[sizeof(topic)-1]=0;
    char * value = (resp == COMMAND_RESP_OK)?"ok":"error";

    _mqtt_send(topic, value, 0);
    _cmd_ready = false;
}

void        esp_comms_power_down(void) {}

bool        esp_comms_get_unix_time(int64_t * ts)
{
    if (!ts)
        false;
    struct timeval tv_now;
    if (gettimeofday(&tv_now, NULL) != 0)
        false;
    *ts = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    return true;
}



bool        esp_comms_send_str(char* str) { return false; }
bool        esp_comms_get_id(char* str, uint8_t len) { return false; }

static void _prep_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != COMMS_TYPE_WIFI)
    {
        memset(comms_config, 0, sizeof(comms_config_t));
        comms_config->type = COMMS_TYPE_WIFI;
    }
}


static command_response_t _get_set_str(unsigned dst_offset, unsigned dst_len, char * src, char * name, bool show, cmd_ctx_t * ctx)
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
            cmd_ctx_out(ctx, "Too long.");
            return COMMAND_RESP_ERR;
        }
        memcpy(((char*)osm_config) + dst_offset, p, len);
    }
    if (show)
        cmd_ctx_out(ctx, "%s: %s", name, p);
    else if (*p)
        cmd_ctx_out(ctx, "%s: is set", name);
    else
        cmd_ctx_out(ctx, "%s: not set", name);

    return COMMAND_RESP_OK;
}


static command_response_t _ssid_cb(char *args, cmd_ctx_t * ctx)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, ssid), SSID_LEN, args, "SSID", true, ctx);
}


static command_response_t _pw_cb(char *args, cmd_ctx_t * ctx)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, password), WFPW_LEN, args, "Password", false, ctx);
}


static void _print_ams(cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx, "Options:");
    for(unsigned n=0; n < ARRAY_SIZE(_authmodes); n++)
        cmd_ctx_out(ctx, "  %s", _authmodes[n].name);
}


static command_response_t _am_cb(char *args, cmd_ctx_t * ctx)
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
                cmd_ctx_out(ctx, "authmode: %s", _authmodes[n].name);
                _print_ams(ctx);
                return COMMAND_RESP_OK;
            }
        }
        _print_ams(ctx);
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
                cmd_ctx_out(ctx, "authmode: %s", am);
                osm_config->authmode = _authmodes[n].authmode;
                return COMMAND_RESP_OK;
            }
        }
        cmd_ctx_out(ctx, "Unknown authmode: %s", am);
        return COMMAND_RESP_ERR;

    }
    return COMMAND_RESP_OK;
}


static command_response_t _mqtt_svr_cb(char *args, cmd_ctx_t * ctx)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr), SVR_LEN, args, "MQTT Broker", true, ctx);
}


static command_response_t _mqtt_usr_cb(char *args, cmd_ctx_t * ctx)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr_user), SVRUSR_LEN, args, "MQTT User", true, ctx);
}


static command_response_t _mqtt_pw_cb(char *args, cmd_ctx_t * ctx)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr_pw), SVRPW_LEN, args, "MQTT Password", false, ctx);
}


static command_response_t _auto_start_cb(char *args, cmd_ctx_t * ctx)
{
    osm_wifi_config_t* osm_config = _wifi_get_config();
    if (*args && osm_config)
        osm_config->autostart = (args[0] == '1') || (tolower(args[0]) == 't');
    if (osm_config)
    {
        cmd_ctx_out(ctx, "Autostart : %u", (unsigned)osm_config->autostart);
        return COMMAND_RESP_OK;
    }
    return COMMAND_RESP_ERR;
}


static command_response_t _fwd_cb(char *args, cmd_ctx_t * ctx)
{
    osm_wifi_config_t* osm_config = _wifi_get_config();
    if (*args && osm_config)
        osm_config->fwd_uart = (args[0] == '1') || (tolower(args[0]) == 't');
    if (osm_config)
    {
        cmd_ctx_out(ctx, "MQTT UART forwarding : %u", (unsigned)osm_config->fwd_uart);
        return COMMAND_RESP_OK;
    }
    return COMMAND_RESP_ERR;
}


static command_response_t _esp_comms_cb(char *args, cmd_ctx_t * ctx)
{
    struct cmd_link_t cmds[] =
    {
        { "wifi_ssid", "Get/Set WiFi SSID",      _ssid_cb             , false , NULL },
        { "wifi_pw",   "Get/Set WiFi Password.", _pw_cb                      , false , NULL },
        { "wifi_am",   "Get/Set WiFi Auth Mode", _am_cb                       , false , NULL },
        { "mqtt_svr",   "Get/Set MQTT broker", _mqtt_svr_cb                       , false , NULL },
        { "mqtt_usr",   "Get/Set MQTT user", _mqtt_usr_cb                       , false , NULL },
        { "mqtt_pw",   "Get/Set MQTT password", _mqtt_pw_cb                       , false , NULL },
        { "autostart",   "Get/Set connection auto start", _auto_start_cb                       , false , NULL },
        { "uart_fwd", "Enable/Disable UART MQTT forwarding", _fwd_cb, false , NULL },
    };
    command_response_t r = COMMAND_RESP_ERR;
    if (args[0])
    {
	char * next = skip_to_space(args);
        if (next[0])
        {
            char * t = next;
            next = skip_space(next);
            *t = 0;
        }
        for(unsigned n=0; n < ARRAY_SIZE(cmds); n++)
        {
            struct cmd_link_t * cmd = &cmds[n];
            if (!strcmp(cmd->key, args))
                return cmd->cb(next, ctx);
        }
    }
    else r = COMMAND_RESP_OK;

    for(unsigned n=0; n < ARRAY_SIZE(cmds); n++)
    {
        struct cmd_link_t * cmd = &cmds[n];
        cmd_ctx_out(ctx, "%10s : %s", cmd->key, cmd->desc);
    }
    return r;
}


static command_response_t _esp_conn_cb(char *args, cmd_ctx_t * ctx)
{
    _wifi_start();
    _mqtt_start();
    if (_has_mqtt)
    {
        cmd_ctx_out(ctx, "1 | Connected");
        return COMMAND_RESP_OK;
    }
    else
    {
        cmd_ctx_out(ctx, "0 | Disconnected");
        return COMMAND_RESP_ERR;
    }
}


struct cmd_link_t* comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_config", "Config comms", _esp_comms_cb, false , NULL },
        { "comms_conn", "Get if connected or not", _esp_conn_cb, false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
