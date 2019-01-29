#include <string.h>

#include "ring.h"
#include "log.h"


bool ring_buf_add(ring_buf_t * ring_buf, char c)
{
    if (ring_buf->full)
    {
        platform_raw_msg("ERROR : Ring full!");
        return false;
    }

    unsigned w_pos = ring_buf->w_pos;

    ring_buf->buf[w_pos++] = c;
    w_pos %= RING_BUF_SIZE;

    ring_buf->w_pos = w_pos;

    if (ring_buf->w_pos == ring_buf->r_pos)
        ring_buf->full = true;

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
    unsigned sec_size = (r_pos < w_pos)?(w_pos - r_pos):(RING_BUF_SIZE - r_pos + w_pos);
    return sec_size;
}


void     ring_buf_read(ring_buf_t * ring_buf, char * buf, unsigned len)
{
    for(unsigned n = 0; n < len; n++)
    {
        unsigned r_pos = ring_buf->r_pos;
        buf[n] = ring_buf->buf[r_pos];
        r_pos++;
        r_pos %= RING_BUF_SIZE;
        ring_buf->r_pos = r_pos;
        ring_buf->full  = false;
    }
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
        r_pos %= RING_BUF_SIZE;

        if (c == '\n' || c == '\r')
        {
            char c = ring_buf->buf[r_pos];
            if (c == '\r' || c == '\n')
            {
                r_pos++;
                r_pos %= RING_BUF_SIZE;
            }

            buf[n] = 0;
            ring_buf->r_pos = r_pos;
            ring_buf->full  = false;
            return n;
        }
        else buf[n] = c;
    }

    if (len == toread)
    {
        buf[len] = 0;
        ring_buf->r_pos = r_pos;
        ring_buf->full  = false;
        return len;
    }

    return 0;
}


void     ring_buf_do_char(ring_buf_t * ring_buf, ring_buf_char_cb cb, void * data)
{
    unsigned r_pos = ring_buf->r_pos;

    char c = ring_buf->buf[r_pos];
    r_pos++;
    r_pos %= RING_BUF_SIZE;

    if (cb(c, data))
    {
        ring_buf->r_pos = r_pos;
        ring_buf->full  = false;
    }
}
