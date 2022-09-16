#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>


#define I2C_SERVER_LOC                          "/tmp/osm/i2c_socket"

#define BUF_SIZ                                 64


static int _socketfd;


static bool _connect(void)
{
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, I2C_SERVER_LOC, sizeof(addr.sun_path));
    _socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(_socketfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        printf("Could not bind the socket.");
        return false;
    }
    return true;
}


int main(int argc, char **argv)
{
    while (!_connect())
    {
        printf("Could not connect...");
        fflush(stdout);
        sleep(1);
        printf(" Retrying...\n");
    }

    char buf[BUF_SIZ] = "10:1:00:[--]";
    int buf_siz = strnlen(buf, BUF_SIZ);
    printf("SEND: %s\n", buf);
    int send_size = send(_socketfd, buf, buf_siz, 0);
    if (send_size != buf_siz)
    {
        printf("Failed to send the correct size for the message. (%d != %d)", send_size, buf_siz);
        return -1;
    }

    int recv_siz = recv(_socketfd, buf, BUF_SIZ, 0);
    if (recv_siz < 0)
    {
        printf("Failed to receive.");
        return -1;
    }
    printf("RECV: %s\n", buf);

    close(_socketfd);

    return 0;
}

