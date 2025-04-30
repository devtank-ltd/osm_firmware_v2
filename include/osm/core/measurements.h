#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include <osm/core/config.h>
#include <osm/comms/comms.h>
#include <osm/core/base_types.h>

#include <osm/core/measurements_mem.h>

#define MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL  (uint32_t)(15 * 1000)
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
    bool                            (* is_enabled_cb)(char* name);                                  // Function to get if it is already enabled.
    measurements_value_type_t       (* value_type_cb)(char* name);                                  // Function to inform measurement of the value type.
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
    uint8_t                 value_type:6;                                 /* measurements_value_type_t */
    uint8_t                 instant_send:1;
    uint8_t                 is_collecting:1;
    uint8_t                 num_samples:7;
    uint8_t                 has_sent:1;
    uint8_t                 num_samples_init:7;
    uint8_t                 _:1;
    uint8_t                 num_samples_collected:7;
    uint8_t                 __:1;
    uint32_t                collection_time_cache;

} measurements_data_t;


typedef bool (*measurements_for_each_cb_t)(measurements_def_t* def, void * data);


bool     measurements_get_measurements_def(char* name, measurements_def_t ** measurements_def, measurements_data_t ** measurements_data);
bool     measurements_for_each(measurements_for_each_cb_t cb, void * data);

bool     measurements_add(measurements_def_t* measurement);
bool     measurements_del(char* name);

bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
bool     measurements_get_interval(char* name, uint8_t * interval);     // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval
bool     measurements_get_samplecount(char* name, uint8_t * samplecount); // How many samples should be taken in each interval

void     measurements_loop_iteration(void);
void     measurements_init(void);

void     measurements_power_mode(measurements_power_mode_t mode);
void     measurements_derive_cc_phase(void);
bool     measurements_send_test(char * name);

extern bool     measurements_enabled;
bool     measurements_get_reading(char* measurement_name, measurements_reading_t* reading, measurements_value_type_t* type);
bool     measurements_reading_to_str(measurements_reading_t* reading, measurements_value_type_t type, char* text, uint8_t len);


bool     model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
void     model_measurements_repopulate(void);

bool     measurements_rename(char* orig_name, char* new_name_raw);

struct cmd_link_t* measurements_add_commands(struct cmd_link_t* tail);
