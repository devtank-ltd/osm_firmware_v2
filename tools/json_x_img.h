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

#include "persist_config_header.h"

extern persist_storage_t osm_config;

extern int write_json_from_img(const char * filename);
extern int read_json_to_img(const char * filename);


extern void log_debug(uint32_t flag, char *fmt, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_out(char *fmt, ...) PRINTF_FMT_CHECK( 1, 2);

extern void log_error(char *fmt, ...) PRINTF_FMT_CHECK( 1, 2);
