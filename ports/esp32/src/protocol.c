#include "log.h"
#include "base_types.h"
#include "persist_config.h"

#include "protocol.h"

typedef struct
{
    char ssid[32];
    char password[32];
} wifi_config_t;


static wifi_config_t* _wifi_get_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != COMMS_TYPE_LW)
    {
        comms_debug("Tried to get config for WiFi but config is not for WiFi.");
        return NULL;
    }
    return (wifi_config_t*)(comms_config->setup);
}


void protocol_system_init(void)
{
    wifi_config_t* wifi_config = _wifi_get_config();
    if (!wifi_config)
        return;
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
    return false;
}

void protocol_loop_iteration(void) {}

bool        protocol_get_id(char* str, uint8_t len) { return false; }
struct cmd_link_t* protocol_add_commands(struct cmd_link_t* tail) { return tail; }

void        protocol_power_down(void) {}
