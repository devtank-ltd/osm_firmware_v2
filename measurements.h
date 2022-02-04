#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "config.h"
#include "lorawan.h"
#include "value.h"
#include "base_types.h"

#include "measurements_mem.h"

#define MEASUREMENTS__HEX_ARRAY_SIZE            (128-LW_HEADER_SIZE-LW_TAIL_SIZE)   /* Hex is double, and LoRaWAN chip only takes 256 bytes at one time.*/


#define MEASUREMENTS__COLLECT_TIME__TEMPERATURE__MS 10000
#define MEASUREMENTS__COLLECT_TIME__HUMIDITY__MS    10000
#define MEASUREMENTS__COLLECT_TIME__HPM__MS         10000


extern uint32_t transmit_interval;


typedef struct
{
    uint16_t collection_time;                           // Time in ms between calling the init function (init_cb) and collecting the value (get_cb)
    bool     (* init_cb)(char* name);                   // Function to start the process of retrieving the data
    bool     (* get_cb)(char* name, value_t* value);    // Function to collect the value
    void     (* acked_cb)();                            // Function to tell subsystem measurement was successfully sent.
} measurement_inf_t;


extern uint16_t measurements_num_measurements(void);
extern char*    measurements_get_name(unsigned index);

extern void     measurements_print(void);

extern bool     measurements_add(measurement_def_t* measurement);
extern bool     measurements_del(char* name);

extern bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_get_interval(char* name, uint8_t * interval);     // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval
extern bool     measurements_get_samplecount(char* name, uint8_t * samplecount); // How many samples should be taken in each interval

extern void     measurements_loop_iteration(void);
extern void     measurements_init(void);
