#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "config.h"
#include "lorawan.h"
#include "value.h"


#define MEASUREMENTS__HEX_ARRAY_SIZE            (128-LW_HEADER_SIZE-LW_TAIL_SIZE)   /* Hex is double, and LoRaWAN chip only takes 256 bytes at one time.*/
#define MEASUREMENTS_MAX_NUMBER                   20


#define MEASUREMENTS__COLLECT_TIME__TEMPERATURE__MS 10000
#define MEASUREMENTS__COLLECT_TIME__HUMIDITY__MS    10000
#define MEASUREMENTS__COLLECT_TIME__HPM__MS         10000


extern uint32_t transmit_interval;


typedef enum
{
    MODBUS        = 1,
    PM10          = 2,
    PM25          = 3,
    CURRENT_CLAMP = 4,
    W1_PROBE      = 5,
    HTU21D_HUM    = 6,
    HTU21D_TMP    = 7,
    BAT_MON       = 8,
} measurement_def_type_t;


typedef struct
{
    char     name[MEASURE_NAME_LEN + 1];                // Name of the measurement
    uint8_t  interval;                                  // System intervals happen every 5 mins, this is how many must pass for measurement to be sent
    uint8_t  samplecount;                               // Number of samples in the interval set. Must be greater than or equal to 1
    uint8_t  type;                                      // measurement_def_type_t
} measurement_def_base_t;


typedef struct
{
    measurement_def_base_t base;
    uint16_t collection_time;                           // Time in ms between calling the init function (init_cb) and collecting the value (get_cb)
    bool     (* init_cb)(char* name);                   // Function to start the process of retrieving the data
    bool     (* get_cb)(char* name, value_t* value);    // Function to collect the value
} measurement_def_t;


extern uint16_t measurements_num_measurements(void);
extern char*    measurements_get_name(unsigned index);

extern void     measurements_print(void);
extern void     measurements_print_persist(void);

extern bool     measurements_add(measurement_def_t* measurement);
extern bool     measurements_del(char* name);

extern bool     measurements_save(void);

extern bool     measurements_set_interval(char* name, uint8_t interval);       // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_get_interval(char* name, uint8_t * interval);     // Interval is time in multiples of transmit interval (default 5m) for the measurements to be sent.
extern bool     measurements_set_samplecount(char* name, uint8_t samplecount); // How many samples should be taken in each interval
extern bool     measurements_get_samplecount(char* name, uint8_t * samplecount); // How many samples should be taken in each interval

extern void     measurements_loop_iteration(void);
extern void     measurements_init(void);
