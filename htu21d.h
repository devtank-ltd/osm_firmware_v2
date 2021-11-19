#ifndef __HDC2080__
#define __HDC2080__

extern void     hdc2080_init();

/* returns degrees * 10 and %RH * 10 */
extern bool hdc2080_read(uint16_t * temp, uint16_t * humidity);

#endif //__HDC2080__
