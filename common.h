#pragma once

extern uint32_t get_since_boot_ms(void);
extern uint32_t since_boot_delta(uint32_t newer, uint32_t older);

extern void tight_loop_contents(void);
extern void loose_loop_contents(void);

extern float Q_rsqrt( float number );
extern int32_t nlz(uint32_t x);
extern int32_t ilog10(uint32_t x);
