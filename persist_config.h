#ifndef __PERSISTENT_CONIFG__
#define __PERSISTENT_CONIFG__

#include <stdint.h>
#include <stdbool.h>

extern void        init_persistent();

extern const char* persistent_get_name();
extern void        persistent_set_name(const char * name[32]);

extern bool        persistent_get_cal(unsigned adc, int32_t * scale, int32_t * offset);
extern bool        persistent_set_cal(unsigned adc, int32_t scale, int32_t offset);


#endif //__PERSISTENT_CONIFG__
