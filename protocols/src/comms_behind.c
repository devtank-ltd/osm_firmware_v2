#include "model_config.h"
#include "protocol.h"
#include "comms.h"

void        protocol_system_init(void)              { comms_init(); }

void        protocol_loop_iteration(void)           { comms_loop_iteration(); }
bool        protocol_send_ready(void)               { return comms_send_ready(); }
bool        protocol_send_allowed(void)             { return comms_send_allowed(); }
void        protocol_reset(void)                    { comms_reset(); }
void        protocol_process(char* message)         { comms_process(message); }
bool        protocol_get_connected(void)            { return comms_get_connected(); }
bool        protocol_get_id(char* str, uint8_t len) { return comms_get_id(str, len); }

struct cmd_link_t* protocol_add_commands(struct cmd_link_t* tail)   { return comms_add_commands(tail); }

void        protocol_power_down(void)   { comms_power_down(); }
