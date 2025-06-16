#include <stdbool.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <unistd.h>

#include <osm/core/w1.h>

#include <osm/core/log.h>
#include "linux.h"


#define W1_SERVER_LOC                           "w1_socket"


static bool _w1_connected = false;
static int  _w1_socketfd = -1;


bool osm_w1_reset(uint8_t index)
{
    if (_w1_connected)
        close(_w1_socketfd);
    char osm_w1_loc[LOCATION_LEN];
    osm_concat_osm_location(osm_w1_loc, LOCATION_LEN, W1_SERVER_LOC);
    _w1_connected = osm_socket_connect(osm_w1_loc, &_w1_socketfd);
    if (!_w1_connected)
        osm_log_error("Fake one-wire failed to connect to socket.");
    return _w1_connected;
}


uint8_t osm_w1_read_byte(uint8_t index)
{
    if (!_w1_connected)
        return 0;
    uint8_t byte;
    int recv_siz = recv(_w1_socketfd, &byte, 1, 0);
    if (recv_siz < 0)
    {
        osm_log_error("Failed to receive.");
        return 0;
    }
    return byte;
}


void osm_w1_send_byte(uint8_t index, uint8_t byte)
{
    if (!_w1_connected)
        return;
    int send_size = send(_w1_socketfd, &byte, 1, 0);
    if (send_size != 1)
    {
        osm_log_error("Sent size was not equal to send packet.");
    }
}


void osm_w1_init(uint8_t index)
{
}


void osm_w1_linux_deinit(void)
{
    if (_w1_connected)
        close(_w1_socketfd);

    char osm_w1_loc[LOCATION_LEN];
    osm_concat_osm_location(osm_w1_loc, LOCATION_LEN, W1_SERVER_LOC);
    unlink(osm_w1_loc);
}


void osm_w1_enable(unsigned io, bool enabled)
{
}
