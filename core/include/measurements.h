#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "config.h"
#include "lorawan.h"
#include "value.h"
#include "base_types.h"

#include "measurements_mem.h"

#define MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL  (uint32_t)15
#define MEASUREMENTS_HEX_ARRAY_SIZE            ((256-LW_HEADER_SIZE-LW_TAIL_SIZE)/2 - 1)   /* Hex is double, and LoRaWAN chip only takes 256 bytes at one time.*/

_Static_assert (MEASUREMENTS_HEX_ARRAY_SIZE * 2 < LW_PAYLOAD_MAX_DEFAULT, "Measurement send data max longer than LoRaWAN payload max.");


extern uint32_t transmit_interval;


typedef enum
{
    MEASUREMENTS_SENSOR_STATE_SUCCESS,
    MEASUREMENTS_SENSOR_STATE_BUSY,
    MEASUREMENTS_SENSOR_STATE_ERROR,
} measurements_sensor_state_t;


typedef enum
{
    MEASUREMENTS_POWER_MODE_AUTO,
    MEASUREMENTS_POWER_MODE_BATTERY,
    MEASUREMENTS_POWER_MODE_PLUGGED,
} measurements_power_mode_t;


typedef struct
{
    measurements_sensor_state_t     (* collection_time_cb)(char* name, uint32_t* collection_time);  // Function to retrieve the time in ms between calling the init function (init_cb) and collecting the value (get_cb)
    measurements_sensor_state_t     (* init_cb)(char* name);                                        // Function to start the process of retrieving the data
    measurements_sensor_state_t     (* get_cb)(char* name, value_t* value);                         // Function to collect the value
    void                            (* acked_cb)(char* name);                                       // Function to tell subsystem measurement was successfully sent.
    measurements_sensor_state_t     (* iteration_cb)(char* name);                                   // Function that iterates between init and get.
} measurements_inf_t;


extern uint16_t measurements_num_measurements(void);
extern char*    measurements_get_name(unsigned index);

extern void     measurements_print(void);

extern bool     measurements_add(measurements_def_t* measurement);
extern bool     measurements_del(char* name);
extern void     measurements_repopulate(void);

extern bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_get_interval(char* name, uint8_t * interval);     // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval
extern bool     measurements_get_samplecount(char* name, uint8_t * samplecount); // How many samples should be taken in each interval

extern void     measurements_loop_iteration(void);
extern void     measurements_init(void);
extern void     measurements_set_debug_mode(bool enable);

extern void     measurements_power_mode(measurements_power_mode_t mode);
extern void     measurements_derive_cc_phase(void);
extern bool     measurements_send_test(void);
