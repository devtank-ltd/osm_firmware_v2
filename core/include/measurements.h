#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "config.h"
#include "comms.h"
#include "base_types.h"

#include "measurements_mem.h"

#define MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL  (uint32_t)15
#define MEASUREMENTS_VALUE_STR_LEN              23

extern uint32_t transmit_interval;


typedef enum
{
    MEASUREMENTS_POWER_MODE_AUTO,
    MEASUREMENTS_POWER_MODE_BATTERY,
    MEASUREMENTS_POWER_MODE_PLUGGED,
} measurements_power_mode_t;


typedef enum
{
    MEASUREMENTS_VALUE_TYPE_INVALID = 0,
    MEASUREMENTS_VALUE_TYPE_I64,
    MEASUREMENTS_VALUE_TYPE_STR,
    MEASUREMENTS_VALUE_TYPE_FLOAT,
} measurements_value_type_t;


typedef struct
{
    measurements_sensor_state_t     (* collection_time_cb)(char* name, uint32_t* collection_time);  // Function to retrieve the time in ms between calling the init function (init_cb) and collecting the value (get_cb)
    measurements_sensor_state_t     (* init_cb)(char* name, bool in_isolation);                     // Function to start the process of retrieving the data
    measurements_sensor_state_t     (* get_cb)(char* name, measurements_reading_t* value);          // Function to collect the value
    void                            (* acked_cb)(char* name);                                       // Function to tell subsystem measurement was successfully sent.
    measurements_sensor_state_t     (* iteration_cb)(char* name);                                   // Function that iterates between init and get.
    void                            (* enable_cb)(char* name, bool enabled);                        // Function to inform measurement if active or not.
} measurements_inf_t;


typedef struct
{
    union
    {
        struct
        {
            int64_t sum;
            int64_t max;
            int64_t min;
        } value_64;
        struct
        {
            int32_t sum;
            int32_t max;
            int32_t min;
        } value_f;
        struct
        {
            char    str[MEASUREMENTS_VALUE_STR_LEN];
        } value_s;
    };
} measurements_value_t;


typedef struct
{
    measurements_value_t    value;
    uint8_t                 value_type:7;                                 /* measurements_value_type_t */
    uint8_t                 is_collecting:1;
    uint8_t                 num_samples;
    uint8_t                 num_samples_init;
    uint8_t                 num_samples_collected;
    uint32_t                collection_time_cache;
} measurements_data_t;


extern uint16_t measurements_num_measurements(void);
extern char*    measurements_get_name(unsigned index);

extern void     measurements_print(void);

extern bool     measurements_add(measurements_def_t* measurement);
extern bool     measurements_del(char* name);

extern bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_get_interval(char* name, uint8_t * interval);     // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval
extern bool     measurements_get_samplecount(char* name, uint8_t * samplecount); // How many samples should be taken in each interval

extern void     measurements_loop_iteration(void);
extern void     measurements_init(void);
extern void     measurements_set_debug_mode(bool enable);

extern void     measurements_power_mode(measurements_power_mode_t mode);
extern void     measurements_derive_cc_phase(void);
extern bool     measurements_send_test(char * name);

extern bool     measurements_enabled;
extern bool     measurements_get_reading(char* measurement_name, measurements_reading_t* reading, measurements_value_type_t* type);
extern bool     measurements_reading_to_str(measurements_reading_t* reading, measurements_value_type_t type, char* text, uint8_t len);


extern bool     measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
extern void     measurements_repopulate(void);
