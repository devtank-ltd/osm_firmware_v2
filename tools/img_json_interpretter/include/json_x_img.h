#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <json-c/json.h>
#include <json-c/json_util.h>


#include "lw.h"

#include "persist_config_header.h"


typedef struct
{
    persist_measurements_storage_t  * measurements;
    unsigned                          measurements_size;
    persist_storage_t               * config;
    unsigned                          config_size;
} json_x_img_mem_t;


struct model_config_funcs_t
{
    char        model_name[MODEL_NAME_LEN];
    unsigned    model_config_size;
    bool        (*write_json_from_img_cb)(struct json_object * root, void* model_config);
    bool        (*read_json_to_img_cb)(struct json_object * root, void* model_config);

    struct model_config_funcs_t * next;
};


extern json_x_img_mem_t osm_mem;

extern int write_json_from_img(const char * filename);
extern int read_json_to_img(const char * filename);


extern void log_debug(uint32_t flag, char *fmt, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_out(char *fmt, ...) PRINTF_FMT_CHECK( 1, 2);

extern void log_error(char *fmt, ...) PRINTF_FMT_CHECK( 1, 2);
