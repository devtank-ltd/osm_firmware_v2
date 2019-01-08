#include <ctype.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "log.h"
#include "cmd.h"


static char     rx_buffer[MAX_LINELEN];
static unsigned rx_buffer_len = 0;
static bool     rx_ready      = false;

static TaskHandle_t h_cmds_task;


bool cmds_add_char(char c)
{
    if (rx_ready)
        return false;

    if (c != '\n' && c != '\r' && rx_buffer_len < MAX_LINELEN - 1)
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
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(500));
        if (rx_ready)
        {
            log_out(rx_buffer);
            rx_buffer_len = 0;
            rx_ready = false;
        }
    }
}


void cmds_init()
{
    xTaskCreate(cmds_task,"CMDS",100,NULL,configMAX_PRIORITIES-2,&h_cmds_task);
}
