#include <string.h>
#include <inttypes.h>

#include <esp_wifi.h>
#include <esp_event.h>

#include <mqtt_client.h>

#include "log.h"
#include "cmd.h"
#include "common.h"
#include "base_types.h"
#include "persist_config.h"

#include "protocol.h"

#define SSID_LEN 16
#define WFPW_LEN 32
#define SVR_LEN 24
#define SVRUSR_LEN 12
#define SVRPW_LEN 32

#define MQTT_DEFAULT_PORT 1883

static char _mac[16] = {0};

static volatile bool _has_ip_addr = false;
static volatile bool _has_mqtt = false;
static volatile int _qos = 1;

static esp_mqtt_client_handle_t _client;

static char _cmd[32];
static volatile bool _cmd_ready = false;

typedef struct
{
    uint8_t  type;
    uint8_t  authmode;
    uint16_t _;
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
            snprintf(topic, sizeof(topic), "/osm/%s/cmd", _mac);
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
                osm_wifi_config_t* osm_config = _wifi_get_config();
                comms_debug("Got IP:"IPSTR, IP2STR(&event->ip_info.ip));
                uint8_t mac[6];
                ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA, mac));
                snprintf(_mac, sizeof(_mac), "%"PRIx8"%"PRIx8"%"PRIx8"%"PRIx8"%"PRIx8"%"PRIx8, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                comms_debug("MAC: %s", _mac);

                _has_ip_addr = true;
                static char uri[128];
                //mqtt://username:password@mqtt.eclipseprojects.io
                snprintf(uri, sizeof(uri), "mqtt://%s:%s@%s", osm_config->svr_user, osm_config->svr_pw, osm_config->svr);

                esp_mqtt_client_config_t mqtt_cfg =
                {
                    .broker.address.uri = uri,
                };
                _client = esp_mqtt_client_init(&mqtt_cfg);
                esp_mqtt_client_register_event(_client, ESP_EVENT_ANY_ID, _mqtt_event_handler, NULL);
                esp_mqtt_client_start(_client);

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

static bool _mqtt_send(const char * name, const char * value, unsigned len)
{
    char topic[64];
    snprintf(topic, sizeof(topic), "/osm/%s/measurements/%s", _mac, name);

    return esp_mqtt_client_publish(_client, topic, value, len, _qos, false) >= 0;
}


static bool _protocol_append_data_type_float(const char * name, int32_t value)
{
    char svalue[16];
    unsigned len = snprintf(svalue, sizeof(svalue), "%"PRId32".%03ld", value/1000, labs(value/1000));
    return _mqtt_send(name, svalue, len);
}


static bool _protocol_append_data_type_i64(const char * name, int64_t value)
{
    char svalue[24];
    unsigned len = snprintf(svalue, sizeof(svalue), "%"PRId64, value);
    return _mqtt_send(name, svalue, len);
}


static bool _protocol_append_value_type_float(const char * name, measurements_data_t* data)
{
    if (data->num_samples == 1)
        return _protocol_append_data_type_float(name, data->value.value_f.sum);
    bool r = true;
    int32_t mean = data->value.value_f.sum / data->num_samples;
    char tmp[MEASURE_NAME_NULLED_LEN + 4];
    r &= _protocol_append_data_type_float(name, mean);
    snprintf(tmp, sizeof(tmp), "%s_min", name);
    r &= _protocol_append_data_type_float(tmp, data->value.value_f.min);
    snprintf(tmp, sizeof(tmp), "%s_max", name);
    r &= _protocol_append_data_type_float(tmp, data->value.value_f.max);
    return r;
}


static bool _protocol_append_value_type_i64(const char * name, measurements_data_t* data)
{
    if (data->num_samples == 1)
        return _protocol_append_data_type_i64(name, data->value.value_f.sum);
    bool r = true;
    int64_t mean = data->value.value_64.sum / data->num_samples;
    char tmp[MEASURE_NAME_NULLED_LEN + 4];
    r &= _protocol_append_data_type_i64(name, mean);
    snprintf(tmp, sizeof(tmp), "%s_min", name);
    r &= _protocol_append_data_type_i64(tmp, data->value.value_64.min);
    snprintf(tmp, sizeof(tmp), "%s_max", name);
    r &= _protocol_append_data_type_i64(tmp, data->value.value_64.max);
    return r;
}



static bool _protocol_append_value_type_str(const char * name, measurements_data_t* data)
{
    return _mqtt_send(name, data->value.value_s.str, 0);
}


bool        protocol_append_measurement(measurements_def_t* def, measurements_data_t* data)
{
    if (!_has_mqtt)
        return false;
    char * name = def->name;

    switch(data->value_type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            return _protocol_append_value_type_i64(name, data);
        case MEASUREMENTS_VALUE_TYPE_STR:
            return _protocol_append_value_type_str(name, data);
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
            return _protocol_append_value_type_float(name, data	);
        default:
            log_error("Unknown type '%"PRIu8"'.", data->value_type);
    }
    return false;
}


bool        protocol_append_instant_measurement(measurements_def_t* def, measurements_reading_t* reading, measurements_value_type_t type) { return false; }
void        protocol_debug(void) {}
void        protocol_send(void) {}
bool        protocol_send_ready(void) { return false; }
bool        protocol_send_allowed(void) { return false; }
void        protocol_reset(void) {}


bool protocol_get_connected(void)
{
    return _has_mqtt;
}


void protocol_loop_iteration(void)
{
    if (!_cmd_ready)
        return;
    cmds_process(_cmd, strlen(_cmd));
    _cmd_ready = false;
}

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


static void _print_ams(void)
{
    log_out("Options:");
    for(unsigned n=0; n < ARRAY_SIZE(_authmodes); n++)
        log_out("  %s", _authmodes[n].name);
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
                _print_ams();
                return COMMAND_RESP_OK;
            }
        }
        _print_ams();
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
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr), SVR_LEN, args, "MQTT Broker", true);
}


static command_response_t _mqtt_usr_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr_user), SVRUSR_LEN, args, "MQTT User", true);
}


static command_response_t _mqtt_pw_cb(char *args)
{
    return _get_set_str(GETOFFSET(osm_wifi_config_t, svr_pw), SVRPW_LEN, args, "MQTT Password", false);
}

static command_response_t _esp_conn_cb(char *args)
{
    if (_has_mqtt)
    {
        comms_debug("1 | Connected");
        return COMMAND_RESP_OK;
    }
    else
    {
        comms_debug("0 | Disconnected");
        return COMMAND_RESP_ERR;
    }
}


struct cmd_link_t* protocol_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "wifi_ssid", "Get/Set WiFi SSID",      _ssid_cb             , false , NULL },
        { "wifi_pw",   "Get/Set WiFi Password.", _pw_cb                      , false , NULL },
        { "wifi_am",   "Get/Set WiFi Auth Mode", _am_cb                       , false , NULL },
        { "mqtt_svr",   "Get/Set MQTT broker", _mqtt_svr_cb                       , false , NULL },
        { "mqtt_usr",   "Get/Set MQTT user", _mqtt_usr_cb                       , false , NULL },
        { "mqtt_pw",   "Get/Set MQTT password", _mqtt_pw_cb                       , false , NULL },
        { "comms_conn", "Get if connected or not", _esp_conn_cb, false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}



