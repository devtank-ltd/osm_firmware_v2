#include <ctype.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "log.h"
#include "cmd.h"
#include "uarts.h"


static char     rx_buffer[CMD_LINELEN];
static unsigned rx_buffer_len = 0;
static bool     rx_ready      = false;

static TaskHandle_t h_cmds_task;


typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(void);
} cmd_t;


static void uart_broadcast_cb();
static void print_systick_cb();


static cmd_t cmds[] = {
    { "uarts", "Broad message to uarts.", uart_broadcast_cb},
    { "systick", "Print current systick.", print_systick_cb},
    { NULL },
};



void uart_broadcast_cb()
{
    for(unsigned n=1; n < 4; n++)
        uart_out(n, "UART-TEST\n");
}


void print_systick_cb()
{
    log_out("Systick : %u", (unsigned)xTaskGetTickCount());
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
        BaseType_t woken = pdFALSE;

        rx_buffer[rx_buffer_len++] = 0;
        rx_ready = true;

        vTaskNotifyGiveFromISR(h_cmds_task, &woken);
        portYIELD_FROM_ISR(woken);
    }

    return true;
}


static void cmds_task(void *args __attribute((unused)))
{
    log_debug("cmds_task");
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        if (rx_ready)
        {
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
    }
}


void cmds_init()
{
    xTaskCreate(cmds_task,"CMDS",100,NULL,configMAX_PRIORITIES-2,&h_cmds_task);
}
