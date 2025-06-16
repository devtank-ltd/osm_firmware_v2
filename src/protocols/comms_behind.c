#include "model_config.h"
#include <osm/protocols/protocol.h>
#include <osm/comms/comms.h>

void        osm_protocol_system_init(void)              { osm_comms_init(); }

void        osm_protocol_loop_iteration(void)           { osm_comms_loop_iteration(); }
bool        osm_protocol_send_ready(void)               { return osm_comms_send_ready(); }
bool        osm_protocol_send_allowed(void)             { return osm_comms_send_allowed(); }
void        osm_protocol_reset(void)                    { osm_comms_reset(); }
void        osm_protocol_process(char* message)         { osm_comms_process(message); }
bool        osm_protocol_get_connected(void)            { return osm_comms_get_connected(); }
bool        osm_protocol_get_id(char* str, uint8_t len) { return osm_comms_get_id(str, len); }

struct cmd_link_t* osm_protocol_add_commands(struct cmd_link_t* tail)   { return osm_comms_add_commands(tail); }

void        osm_protocol_power_down(void)   { osm_comms_power_down(); }
