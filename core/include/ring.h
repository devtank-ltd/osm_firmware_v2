#pragma once

#include <stdbool.h>
#include "config.h"

typedef struct
{
    volatile char *   buf;
    unsigned          size;
    volatile unsigned r_pos;
    volatile unsigned w_pos;
} ring_buf_t;

#define RING_BUF_INIT(_buf_, _size_) {_buf_, _size_, 0, 0}

static inline void ring_buf_clear(ring_buf_t * ring_buf) { ring_buf->r_pos = 0; ring_buf->w_pos = 0; }

extern bool     ring_buf_add(ring_buf_t * ring_buf, char c);
extern bool     ring_buf_add_data(ring_buf_t * ring_buf, void * data, unsigned size);
extern void     ring_buf_add_str(ring_buf_t * ring_buf, char * s);
extern unsigned ring_buf_get_pending(ring_buf_t * ring_buf);

extern bool     ring_buf_is_full(ring_buf_t * ring_buf);

extern unsigned ring_buf_read(ring_buf_t * ring_buf, char * buf, unsigned len);
extern unsigned ring_buf_discard(ring_buf_t * ring_buf, unsigned len);
extern unsigned ring_buf_peek(ring_buf_t * ring_buf, char * buf, unsigned len);
extern unsigned ring_buf_readline(ring_buf_t * ring_buf, char * buf, unsigned len);

typedef unsigned (*ring_buf_consume_cb)(char * buf, unsigned len, void *data);

extern unsigned ring_buf_consume(ring_buf_t * ring_buf, ring_buf_consume_cb cb, char * tmp_buf, unsigned len, void * data);

