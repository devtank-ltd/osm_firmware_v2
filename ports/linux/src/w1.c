#include <stdbool.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <unistd.h>

#include "w1.h"

#include "log.h"
#include "linux.h"


#define W1_SERVER_LOC                           "/tmp/osm/w1_socket"


static bool _w1_connected = false;
static int  _w1_socketfd;


bool w1_reset(uint8_t index)
{
    if (_w1_connected)
        close(_w1_socketfd);
    _w1_connected = socket_connect(W1_SERVER_LOC, &_w1_socketfd);
    if (!_w1_connected)
        log_error("Fake one-wire failed to connect to socket.");
    return _w1_connected;
}


uint8_t w1_read_byte(uint8_t index)
{
    if (!_w1_connected)
        return 0;
    uint8_t byte;
    int recv_siz = recv(_w1_socketfd, &byte, 1, 0);
    if (recv_siz < 0)
    {
        log_error("Failed to receive.");
        return 0;
    }
    return byte;
}


void w1_send_byte(uint8_t index, uint8_t byte)
{
    if (!_w1_connected)
        return;
    int send_size = send(_w1_socketfd, &byte, 1, 0);
    if (send_size != 1)
    {
        log_error("Sent size was not equal to send packet.");
    }
}


void w1_init(uint8_t index)
{
}


void w1_linux_deinit(void)
{
    if (_w1_connected)
        close(_w1_socketfd);
}
