#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "lorawan.h"


#define MEASUREMENTS__HEX_ARRAY_SIZE            26


typedef int64_t value_t;


typedef struct
{
    char     name[16];                      // Name of the measurement
    uint16_t lora_id;                       // ID of the measurement found in the LoRaWAN protocol
    uint16_t collection_time;               // Time in ms between calling the init function (init_cb) and collecting the value (get_cb)
    uint8_t  interval;                      // System intervals happen every 5 mins, this is how many must pass for measurement to be sent
    uint8_t  samplecount;                   // Number of samples in the interval set. Must be greater than or equal to 1
    bool     (*init_cb)(void);              // Function to start the process of retrieving the data
    bool     (*get_cb)(value_t* value);     // Function to collect the value
} measurement_def_t;


extern uint16_t measurements_num_measurements(void);
extern char*    measurements_get_name(unsigned index);

extern bool     measurements_add(measurement_def_t* measurement);
extern bool     measurements_del(char* name);

extern bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of 5m for the measurements to be sent.
extern bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval

extern void     measurements_loop_iteration(void);
extern void     measurements_init(void);
