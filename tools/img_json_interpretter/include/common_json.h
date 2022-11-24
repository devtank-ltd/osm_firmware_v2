#pragma once

#include "json_x_img.h"

#include "modbus_mem.h"
#include "cc.h"
#include "ftma.h"


int                 get_defaulted_int(struct json_object * root, char * name, int default_value);
double              get_defaulted_double(struct json_object * root, char * name, float default_value);
bool                get_string_buf(struct json_object * root, char * name, char * buf, unsigned len);
void                measurement_del(const char * name);
measurements_def_t* measurement_get_free(measurements_def_t * measurement_arr);
uint16_t            get_io_pull(const char * str, uint16_t current);
uint16_t            get_io_dir(const char * str, uint16_t current);
bool                read_modbus_json(struct json_object * root, modbus_bus_t* modbus_bus);
bool                read_measurements_json(struct json_object * root);
bool                read_ios_json(struct json_object * root, uint16_t* ios_state);
bool                read_cc_configs_json(struct json_object * root, cc_config_t* cc_config);
bool                read_ftma_configs_json(struct json_object * root, ftma_config_t* ftma_configs);
void                free_osm_mem(void);
bool                model_config_get(char* model_name, struct model_config_funcs_t ** funcs);
bool                write_modbus_json(struct json_object * root, modbus_bus_t* modbus_bus);
void                write_measurements_json(struct json_object * root);
void                write_ios_json(struct json_object * root, uint16_t* ios_state);
void                write_cc_config_json(struct json_object * root, cc_config_t* cc_configs, unsigned cc_count);
bool                write_ftma_config_json(struct json_object * root, ftma_config_t* ftma_configs, unsigned ftma_count);
