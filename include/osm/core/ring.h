#pragma once

#include <stdbool.h>
#include <osm/core/config.h>

typedef struct
{
    volatile char *   buf;
    unsigned          size;
    volatile unsigned r_pos;
    volatile unsigned w_pos;
} osm_ring_buf_t;

#define RING_BUF_INIT(_buf_, _size_) {_buf_, _size_, 0, 0}

static inline void ring_buf_clear(osm_ring_buf_t * ring_buf) { ring_buf->r_pos = 0; ring_buf->w_pos = 0; }

bool     osm_ring_buf_add(osm_ring_buf_t * ring_buf, char c);
bool     osm_ring_buf_add_data(osm_ring_buf_t * ring_buf, void * data, unsigned size);
void     osm_ring_buf_add_str(osm_ring_buf_t * ring_buf, char * s);
unsigned osm_ring_buf_get_pending(osm_ring_buf_t * ring_buf);

bool     osm_ring_buf_is_full(osm_ring_buf_t * ring_buf);

unsigned osm_ring_buf_read(osm_ring_buf_t * ring_buf, char * buf, unsigned len);
unsigned osm_ring_buf_discard(osm_ring_buf_t * ring_buf, unsigned len);
unsigned osm_ring_buf_peek(osm_ring_buf_t * ring_buf, char * buf, unsigned len);
unsigned osm_ring_buf_readline(osm_ring_buf_t * ring_buf, char * buf, unsigned len);

typedef unsigned (*ring_buf_consume_cb)(char * buf, unsigned len, void *data);

unsigned osm_ring_buf_consume(osm_ring_buf_t * ring_buf, ring_buf_consume_cb cb, char * tmp_buf, unsigned len, void * data);

