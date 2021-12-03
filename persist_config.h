#ifndef __PERSISTENT_CONIFG__
#define __PERSISTENT_CONIFG__

#include <stdint.h>
#include <stdbool.h>

extern void init_persistent();
extern void persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);
extern void persist_set_lw_dev_eui(char* dev_eui);
extern bool persist_get_lw_dev_eui(char** dev_eui);
extern void persist_set_lw_app_key(char* app_key);
extern bool persist_get_lw_app_key(char** app_key);
extern bool persist_set_interval(uint8_t uuid, uint16_t interval);
extern bool persist_get_interval(uint8_t uuid, uint16_t* interval);
extern void persist_new_interval(uint8_t uuid, uint16_t interval);
extern bool persist_set_samplerate(uint8_t uuid, uint16_t samplerate);
extern bool persist_get_samplerate(uint8_t uuid, uint16_t* samplerate);
extern void persist_new_samplerate(uint8_t uuid, uint16_t samplerate);

typedef struct
{
    uint32_t baudrate;
    uint8_t binary_protocol; /* BIN or RTU */
    uint8_t databits;        /* 8? */
    uint8_t stopbits;        /* uart_stop_bits_t */
    uint8_t parity;          /* uart_parity_t */
} __attribute__((__packed__)) modbus_bus_config_t;


extern void persist_set_modbus_bus_config(modbus_bus_config_t* config);
extern bool persist_get_modbus_bus_config(modbus_bus_config_t* config);

#endif //__PERSISTENT_CONIFG__
