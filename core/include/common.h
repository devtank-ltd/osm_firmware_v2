#pragma once

#include "config.h"
#include "w1.h"
#include "pulsecount.h"


_Static_assert(STRLEN(GIT_VERSION)-3, "No git commit.");


typedef struct
{
    char        name[MEASURE_NAME_LEN];
    unsigned    io;
} special_io_info_t;


extern bool     msg_is(const char* ref, char* message);

extern uint32_t get_since_boot_ms(void);
extern uint32_t since_boot_delta(uint32_t newer, uint32_t older);

extern float    Q_rsqrt( float number );
extern int32_t  nlz(uint32_t x);
extern int32_t  ilog10(uint32_t x);
extern uint64_t abs_i64(int64_t val);
extern bool     u64_multiply_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2);
extern bool     u64_addition_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2);
extern void     spin_blocking_ms(uint32_t ms);
extern bool     main_loop_iterate_for(uint32_t timeout, bool (*should_exit_db)(void *userdata),  void *userdata);
