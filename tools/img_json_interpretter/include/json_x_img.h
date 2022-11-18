#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <json-c/json.h>
#include <json-c/json_util.h>

#include "pinmap.h"
#include "modbus_mem.h"
#include "lw.h"
#include "cc.h"

#include "persist_config_header.h"


typedef struct
{
    persist_measurements_storage_t  measurements    __attribute__((aligned (2048)));
    persist_storage_t               config          __attribute__((aligned (2048)));
} json_x_img_mem_t;


extern json_x_img_mem_t osm_mem;

extern int write_json_from_img(const char * filename);
extern int read_json_to_img(const char * filename);


extern void log_debug(uint32_t flag, char *fmt, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_out(char *fmt, ...) PRINTF_FMT_CHECK( 1, 2);

extern void log_error(char *fmt, ...) PRINTF_FMT_CHECK( 1, 2);
