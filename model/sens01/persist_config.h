#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "base_types.h"
#include "measurements.h"
#include "pinmap.h"


extern void persistent_init(void);

extern comms_config_t* persist_get_comms_config(void);

extern measurements_def_t * persist_get_measurements(void);
extern bool persist_get_mins_interval(uint32_t * mins_interval);
extern bool persist_set_mins_interval(uint32_t mins_interval);

extern float * persist_get_sai_cal_coeffs(void);

extern uint16_t* persist_get_ios_state(void);

extern modbus_bus_t * persist_get_modbus_bus(void);

extern ftma_config_t* persist_get_fmta_config(void);
