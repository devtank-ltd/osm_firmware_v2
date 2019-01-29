#ifndef __RING__
#define __RING__

#include <stdbool.h>
#include "config.h"

typedef struct
{
    volatile char     buf[RING_BUF_SIZE];
    volatile unsigned r_pos;
    volatile unsigned w_pos;
    volatile bool     full;
} ring_buf_t;

#define RING_BUF_INIT { {0}, 0, 0, false}

extern bool     ring_buf_add(ring_buf_t * ring_buf, char c);
extern void     ring_buf_add_str(ring_buf_t * ring_buf, char * s);
extern unsigned ring_buf_get_pending(ring_buf_t * ring_buf);


extern void     ring_buf_read(ring_buf_t * ring_buf, char * buf, unsigned len);
extern unsigned ring_buf_readline(ring_buf_t * ring_buf, char * buf, unsigned len);

typedef bool (*ring_buf_char_cb)(char c, void *data);

extern void     ring_buf_do_char(ring_buf_t * ring_buf, ring_buf_char_cb cb, void * data);


#endif //__RING__
