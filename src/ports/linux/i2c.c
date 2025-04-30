#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/socket.h>

#include <osm/core/i2c.h>

#include <osm/core/log.h>
#include "linux.h"


#define I2C_SERVER_LOC                          "i2c_socket"

#define I2C_BUF_SIZ                     1024


static bool _i2c_connected = false;
static int  _i2c_socketfd = -1;


void i2cs_init(void)
{
    char osm_i2c_loc[LOCATION_LEN];
    concat_osm_location(osm_i2c_loc, LOCATION_LEN, I2C_SERVER_LOC);
    _i2c_connected = socket_connect(osm_i2c_loc, &_i2c_socketfd);
    if (!_i2c_connected)
    {
        log_error("Fake I2C Failed to connect to socket.");
        return;
    }
}


void i2c_linux_deinit(void)
{
    if (_i2c_connected)
        close(_i2c_socketfd);

    char osm_i2c_loc[LOCATION_LEN];
    concat_osm_location(osm_i2c_loc, LOCATION_LEN, I2C_SERVER_LOC);
    unlink(osm_i2c_loc);
}


bool i2c_transfer_timeout(uint32_t i2c, uint8_t addr, const uint8_t *w, unsigned wn, uint8_t *r, unsigned rn, unsigned timeout_ms)
{
    if ((!w && wn) || (!r && rn))
    {
        log_error("Handed NULL pointer.");
        return false;
    }
    if (!_i2c_connected)
    {
        log_error("Fake I2C is not connected.");
        return false;
    }

    char buf[I2C_BUF_SIZ];
    char tmp_buf[I2C_BUF_SIZ];
    snprintf(buf, I2C_BUF_SIZ-1, "%.02x:%x", addr, wn);
    if (wn)
    {
        strncat(buf, "[", I2C_BUF_SIZ-1);
        for (unsigned i = 0; i < wn; i++)
        {
            if (i)
                strncat(buf, ",", I2C_BUF_SIZ-1);
            snprintf(tmp_buf, I2C_BUF_SIZ-1, "%.02x", w[i]);
            strncat(buf, tmp_buf, I2C_BUF_SIZ-1);
        }
        strncat(buf, "]", I2C_BUF_SIZ-1);
    }
    snprintf(tmp_buf, I2C_BUF_SIZ-1, ":%x", rn);
    strncat(buf, tmp_buf, I2C_BUF_SIZ-1);

    int buf_siz = strnlen(buf, I2C_BUF_SIZ-1);
    int send_size = send(_i2c_socketfd, buf, buf_siz, 0);
    if (send_size != buf_siz)
    {
        log_error("Failed to send the correct size I2C for the message. (%d != %d)", send_size, buf_siz);
        return false;
    }
    linux_port_debug("I2C sent %d", send_size);

    int recv_siz = recv(_i2c_socketfd, buf, I2C_BUF_SIZ-1, 0);
    if (recv_siz < 0)
    {
        log_error("Failed to receive.");
        return false;
    }

    linux_port_debug("I2C received %d", recv_siz);

    buf[recv_siz] = 0;
    char* pos = buf;
    char* npos;
    uint16_t addr_local = strtoul(pos, &npos, 16);
    if (addr != addr_local)
        goto recv_bad_exit;

    if (pos == npos || npos != strchr(pos, ':'))
        goto recv_bad_fmt_exit;
    pos = npos + 1;

    uint16_t wn_local = strtoul(pos, &npos, 16);
    if (pos == npos || *npos != ':')
        goto recv_bad_fmt_exit;
    pos = npos + 1;

    if (wn_local != wn)
        goto recv_bad_exit;

    uint16_t rn_local = strtoul(pos, &npos, 16);
    if (pos == npos)
        goto recv_bad_fmt_exit;
    pos = npos;

    if (rn_local != rn)
        goto recv_bad_exit;

    if (*pos == '[' && rn)
    {
        pos += 1;
        for (unsigned i = 0; i < rn; i++)
        {
            r[i] = strtoul(pos, &npos, 16);
            if (pos == npos)
                goto recv_bad_fmt_exit;
            pos = npos;
            if ((i < rn - 1) && (*pos != ','))
                goto recv_bad_fmt_exit;
            pos++;
        }
        pos--;

        if (*pos != ']' && *(pos+1) != '\0')
            goto recv_bad_fmt_exit;

        pos += 2;
        return true;
    }
    else if (*pos == '\0' && !rn)
        return true;
    else
        goto recv_bad_fmt_exit;

recv_bad_fmt_exit:
    log_error("Received bad format. (%s)", buf);
    return false;
recv_bad_exit:
    log_error("Received message doesn't match sent. (%s)", buf);
    return false;
}
