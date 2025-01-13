#include <inttypes.h>
#include <stdbool.h>

#ifndef comms_name
#define comms_name          rp2350_comms
#endif // comms_name


bool rp2350_comms_send(char* data, uint16_t len);
void rp2350_comms_init(void);
bool rp2350_comms_send_ready(void);
bool rp2350_comms_send_allowed(void);
void rp2350_comms_reset(void);
bool rp2350_comms_get_connected(void);
void rp2350_comms_process(char* message);
void rp2350_comms_loop_iteration(void);
void rp2350_comms_power_down(void);
bool rp2350_comms_get_unix_time(int64_t * ts);
bool rp2350_comms_send_str(char* str);
bool rp2350_comms_persist_config_cmp(void* d0, void* d1);
bool rp2350_comms_get_id(char* str, uint8_t len);
