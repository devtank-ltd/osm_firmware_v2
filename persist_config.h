#ifndef __PERSISTENT_CONIFG__
#define __PERSISTENT_CONIFG__

#include <stdint.h>
#include <stdbool.h>

extern void        init_persistent();

extern const char* persistent_get_name();
extern void        persistent_set_name(const char * name);

extern bool        persistent_get_cal(unsigned adc, uint64_t * scale, uint64_t * offset);
extern bool        persistent_set_cal(unsigned adc, uint64_t scale, uint64_t offset);


#endif //__PERSISTENT_CONIFG__
