#include "protocol.h"

void protocol_system_init(void) {}
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
