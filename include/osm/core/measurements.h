#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include <osm/core/config.h>
#include <osm/comms/comms.h>
#include <osm/core/base_types.h>

#include <osm/core/measurements_mem.h>

#define OSM_MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL  (uint32_t)(15 * 1000)
#define OSM_MEASUREMENTS_VALUE_STR_LEN              23

extern uint32_t transmit_interval;


typedef enum
{
    OSM_MEASUREMENTS_POWER_MODE_AUTO,
    OSM_MEASUREMENTS_POWER_MODE_BATTERY,
    OSM_MEASUREMENTS_POWER_MODE_PLUGGED,
} osm_measurements_power_mode_t;


typedef enum
{
    OSM_MEASUREMENTS_VALUE_TYPE_INVALID = 0,
    OSM_MEASUREMENTS_VALUE_TYPE_I64,
    OSM_MEASUREMENTS_VALUE_TYPE_STR,
    OSM_MEASUREMENTS_VALUE_TYPE_FLOAT,
} osm_measurements_value_type_t;


typedef struct
{
    osm_measurements_sensor_state_t     (* collection_time_cb)(char* name, uint32_t* collection_time);  // Function to retrieve the time in ms between calling the init function (init_cb) and collecting the value (get_cb)
    osm_measurements_sensor_state_t     (* init_cb)(char* name, bool in_isolation);                     // Function to start the process of retrieving the data
    osm_measurements_sensor_state_t     (* get_cb)(char* name, measurements_reading_t* value);          // Function to collect the value
    void                            (* acked_cb)(char* name);                                       // Function to tell subsystem measurement was successfully sent.
    osm_measurements_sensor_state_t     (* iteration_cb)(char* name);                                   // Function that iterates between init and get.
    void                            (* enable_cb)(char* name, bool enabled);                        // Function to inform measurement if active or not.
    bool                            (* is_enabled_cb)(char* name);                                  // Function to get if it is already enabled.
    osm_measurements_value_type_t       (* value_type_cb)(char* name);                                  // Function to inform measurement of the value type.
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
            char    str[OSM_MEASUREMENTS_VALUE_STR_LEN];
        } value_s;
    };
} measurements_value_t;


typedef struct
{
    measurements_value_t    value;
    uint8_t                 value_type:6;                                 /* osm_measurements_value_type_t */
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


bool     osm_measurements_get_measurements_def(char* name, measurements_def_t ** measurements_def, measurements_data_t ** measurements_data);
bool     osm_measurements_for_each(measurements_for_each_cb_t cb, void * data);

bool     osm_measurements_add(measurements_def_t* measurement);
bool     osm_measurements_del(char* name);

bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
bool     measurements_get_interval(char* name, uint8_t * interval);     // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval
bool     measurements_get_samplecount(char* name, uint8_t * samplecount); // How many samples should be taken in each interval

void     osm_measurements_loop_iteration(void);
void     osm_measurements_init(void);

void     osm_measurements_power_mode(osm_measurements_power_mode_t mode);
void     osm_measurements_derive_cc_phase(void);
bool     osm_measurements_send_test(char * name);

extern bool     measurements_enabled;
bool     osm_measurements_get_reading(char* measurement_name, measurements_reading_t* reading, osm_measurements_value_type_t* type);
bool     osm_measurements_reading_to_str(measurements_reading_t* reading, osm_measurements_value_type_t type, char* text, uint8_t len);


bool     osm_model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
void     osm_model_measurements_repopulate(void);

bool     osm_measurements_rename(char* orig_name, char* new_name_raw);

struct cmd_link_t* osm_measurements_add_commands(struct cmd_link_t* tail);
