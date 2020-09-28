#ifndef __PERSISTENT_CONIFG__
#define __PERSISTENT_CONIFG__

#include <stdint.h>
#include <stdbool.h>
#include "basic_fixed.h"

extern void        init_persistent();

extern const char* persistent_get_name();
extern void        persistent_set_name(const char * name);

extern bool        persistent_get_cal(unsigned adc, basic_fixed_t * scale, basic_fixed_t * offset);
extern bool        persistent_set_cal(unsigned adc, basic_fixed_t * scale, basic_fixed_t * offset);



#endif //__PERSISTENT_CONIFG__
