#include <stdlib.h>
#include <string.h>

#include "ring.h"
#include "log.h"



bool ring_buf_add(ring_buf_t * ring_buf, char c)
{
    /* So we know it's got data, we never let write pos catch read pos*/
    unsigned w_pos = ring_buf->w_pos;
    unsigned w_next = (w_pos + 1) % ring_buf->size;

    if (w_next == ring_buf->r_pos)
        return false;

    ring_buf->buf[w_pos] = c;

    ring_buf->w_pos = w_next;

    return true;
}


bool     ring_buf_add_data(ring_buf_t * ring_buf, void * data, unsigned size)
{
    char * pos = (char*)data;
    while(size--)
        if (!ring_buf_add(ring_buf, *pos++))
            return false;
    return true;
}


void ring_buf_add_str(ring_buf_t * ring_buf, char * s)
{
    while(*s)
        ring_buf_add(ring_buf, *s++);
}



unsigned ring_buf_get_pending(ring_buf_t * ring_buf)
{
    unsigned r_pos = ring_buf->r_pos;
    unsigned w_pos = ring_buf->w_pos;
    if (r_pos == w_pos)
        return 0;
    unsigned sec_size = (r_pos < w_pos)?(w_pos - r_pos):(ring_buf->size - r_pos + w_pos);
    return sec_size;
}


bool      ring_buf_is_full(ring_buf_t * ring_buf)
{
    /* So we know it's got data, we never let write pos catch read pos*/
    unsigned w_next = (ring_buf->w_pos + 1) % ring_buf->size;
    return (w_next == ring_buf->r_pos);
}


unsigned  ring_buf_read(ring_buf_t * ring_buf, char * buf, unsigned len)
{
    for(unsigned n = 0; n < len; n++)
    {
        unsigned r_pos = ring_buf->r_pos;
        buf[n] = ring_buf->buf[r_pos];
        if (r_pos == ring_buf->w_pos)
            return n;
        r_pos++;
        r_pos %= ring_buf->size;
        ring_buf->r_pos = r_pos;
    }
    return len;
}


unsigned ring_buf_discard(ring_buf_t * ring_buf, unsigned len)
{
    unsigned r_pos = ring_buf->r_pos;

    for(unsigned n = 0; n < len; n++)
    {
        if (r_pos == ring_buf->w_pos)
            return n;
        r_pos++;
        r_pos %= ring_buf->size;
        ring_buf->r_pos = r_pos;
    }
    return len;

}


unsigned ring_buf_peek(ring_buf_t * ring_buf, char * buf, unsigned len)
{
    unsigned r_pos = ring_buf->r_pos;

    for(unsigned n = 0; n < len; n++)
    {
        buf[n] = ring_buf->buf[r_pos];
        if (r_pos == ring_buf->w_pos)
            return n;
        r_pos++;
        r_pos %= ring_buf->size;
    }
    return len;
}



unsigned  ring_buf_readline(ring_buf_t * ring_buf, char * buf, unsigned len)
{
    unsigned toread = ring_buf_get_pending(ring_buf);

    if (!toread)
        return 0;

    len-=1;

    toread = (toread > len)?len:toread;

    unsigned r_pos = ring_buf->r_pos;

    for(unsigned n = 0; n < toread; n++)
    {
        char c = ring_buf->buf[r_pos];
        r_pos++;
        r_pos %= ring_buf->size;

        if (c == '\n' || c == '\r')
        {
            if ((n + 1) < toread)
            {
                c = ring_buf->buf[r_pos];
                if (c == '\r' || c == '\n')
                {
                    r_pos++;
                    r_pos %= ring_buf->size;
                }
            }

            buf[n]          = 0;
            ring_buf->r_pos = r_pos;
            return n;
        }
        else buf[n] = c;
    }

    if (len == toread)
    {
        buf[len]        = 0;
        ring_buf->r_pos = r_pos;
        return len;
    }

    return 0;
}


unsigned     ring_buf_consume(ring_buf_t * ring_buf, ring_buf_consume_cb cb, char * tmp_buf, unsigned len, void * data)
{
    unsigned r_pos = ring_buf->r_pos;

    for(unsigned n = 0; n < len; n++)
    {
        if (r_pos == ring_buf->w_pos)
        {
            len = n;
            break;
        }
        tmp_buf[n] = ring_buf->buf[r_pos];
        r_pos++;
        r_pos %= ring_buf->size;
    }

    if (!len)
        return 0;

    unsigned consumed = cb(tmp_buf, len, data);

    if (consumed < len)
    {
        unsigned unconsumed = len - consumed;
        while(unconsumed--)
        {
            if (r_pos)
                r_pos--;
            else
                r_pos = ring_buf->size-1;
        }
    }
    ring_buf->r_pos = r_pos;
    return consumed;
}
