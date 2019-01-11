#include <stdbool.h>
#include <ctype.h>
#include <string.h>


#include "log.h"
#include "cmd.h"
#include "uarts.h"


static char              rx_buffer[CMD_LINELEN];
static unsigned volatile rx_buffer_len = 0;
static bool     volatile rx_ready      = false;


typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(void);
} cmd_t;


static void uart_broadcast_cb();


static cmd_t cmds[] = {
    { "uarts", "Broad message to uarts.", uart_broadcast_cb},
    { NULL },
};



void uart_broadcast_cb()
{
    for(unsigned n=1; n < 4; n++)
        uart_out(n, "UART-TEST\n");
}




bool cmds_add_char(char c)
{
    if (rx_ready)
        return false;

    if (c != '\n' && c != '\r' && rx_buffer_len < CMD_LINELEN - 1)
    {
        rx_buffer[rx_buffer_len++] = tolower(c);
    }
    else
    {
        rx_buffer[rx_buffer_len++] = 0;
        rx_ready = true;
    }

    return true;
}


void cmds_process()
{
    if (!rx_ready)
        return;

    bool found = false;
    for(cmd_t * cmd = cmds; cmd->key; cmd++)
    {
        if(!strcmp(cmd->key, rx_buffer))
        {
            found = true;
            cmd->cb();
            break;
        }
    }
    if (!found)
    {
        log_out("Unknown command \"%s\"", rx_buffer);
        log_out("======================");
        for(cmd_t * cmd = cmds; cmd->key; cmd++)
            log_out("%10s : %s", cmd->key, cmd->desc);
        log_out("======================");
    }
    rx_buffer_len = 0;
    rx_ready = false;
}


void cmds_init()
{
}
