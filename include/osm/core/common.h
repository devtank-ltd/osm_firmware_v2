#pragma once

#include <osm/core/config.h>
#include <osm/core/w1.h>
#include <osm/sensors/pulsecount.h>


_Static_assert(OSM_STRLEN(GIT_VERSION)-3, "No git commit.");


typedef struct
{
    char        name[OSM_MEASURE_NAME_LEN];
    unsigned    io;
} osm_special_io_info_t;


bool     osm_msg_is(const char* ref, char* message);

uint32_t osm_get_since_boot_ms(void);
uint32_t osm_since_boot_delta(uint32_t newer, uint32_t older);

float    osm_Q_rsqrt( float number );
int32_t  osm_nlz(uint32_t x);
int32_t  osm_ilog10(uint32_t x);
uint64_t osm_abs_i64(int64_t val);
bool     osm_u64_multiply_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2);
bool     osm_u64_addition_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2);
void     osm_spin_blocking_ms(uint32_t ms);
bool     osm_main_loop_iterate_for(uint32_t timeout, bool (*should_exit_db)(void *userdata),  void *userdata);
int32_t  osm_to_f32_from_float(float in);
int32_t  osm_to_f32_from_double(double in);

bool osm_log_out_drain(uint32_t timeout);

struct osm_cmd_link_t* osm_add_commands(struct osm_cmd_link_t* tail, struct osm_cmd_link_t* cmds, unsigned num_cmds);


void osm_cmd_ctx_out(osm_cmd_ctx_t* ctx, const char* fmt, ...);
void osm_cmd_ctx_error(osm_cmd_ctx_t* ctx, const char* fmt, ...);
void osm_cmd_ctx_flush(osm_cmd_ctx_t* ctx);

bool osm_str_is_valid_ascii(char* str, unsigned max_len, bool required);
bool osm_is_str(const char* ref, char* cmp, unsigned cmplen);
