#pragma once

extern uint32_t get_since_boot_ms(void);
extern uint32_t since_boot_delta(uint32_t newer, uint32_t older);

extern float    Q_rsqrt( float number );
extern int32_t  nlz(uint32_t x);
extern int32_t  ilog10(uint32_t x);
extern uint64_t abs_i64(int64_t val);
extern bool     u64_multiply_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2);
extern bool     u64_addition_overflow_check(uint64_t* result, uint64_t arg_1, uint64_t arg_2);
