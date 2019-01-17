#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <libopencm3/stm32/gpio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "pinmap.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "usb_uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "inputs.h"
#include "outputs.h"

static TaskHandle_t      h_blinky = 0;

static char              rx_buffer[CMD_LINELEN];
static unsigned volatile rx_buffer_len = 0;
static bool     volatile rx_ready      = false;
static unsigned          rx_pos        = 0;


#define PSP_ADC_INDEX 5



typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(void);
} cmd_t;


static void uart_broadcast_cb();
static void debug_cb();
static void pps_cb();
static void set_cb();
static void clear_cb();


static cmd_t cmds[] = {
    { "uarts",  "Broad message to uarts.", uart_broadcast_cb},
    { "debug",  "Toggle debug",            debug_cb},
    { "pps",    "Print pulse info.",       pps_cb},
    { "adc",    "Print all ADCs.",         adcs_log},
    { "inputs", "Print all inputs.",       inputs_log},
    { "outputs","Print all inputs.",       outputs_log},
    { "set",    "Turn on output",          set_cb},
    { "clear",  "Turn off output",         clear_cb},
    { NULL },
};



void uart_broadcast_cb()
{
    uart_out(1, "High\n\r", 6);
    uart_out(2, "Low1\n\r", 6);
    uart_out(3, "Low2\n\r", 6);

    usb_uart_send(0, "High\n\r", 6);
    usb_uart_send(1, "Low1\n\r", 6);
    usb_uart_send(2, "Low2\n\r", 6);
}


void debug_cb()
{
    log_show_debug = !log_show_debug;
}


void pps_cb()
{
    unsigned pps1, min_v, max_v, pps2;

    pulsecount_1_get(&pps1);
    pulsecount_2_get(&pps2);

    adcs_get(PSP_ADC_INDEX, &min_v, &max_v, NULL);

    log_out("pulsecount 1: %u  %u %u", pps1, max_v, min_v);
    log_out("pulsecount 2 : %u", pps2);
}


void set_cb()
{
    unsigned output = strtoul(rx_buffer + rx_pos, NULL, 10);
    log_out("Set output %u to ON", output);
    outputs_set(output, true);
}


void clear_cb()
{
    unsigned output = strtoul(rx_buffer + rx_pos, NULL, 10);
    log_out("Set output %u to OFF", output);
    outputs_set(output, false);
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
        vTaskNotifyGiveFromISR(h_blinky,&woken);
        portYIELD_FROM_ISR(woken);

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
        unsigned keylen = strlen(cmd->key);
        if(rx_buffer_len >= keylen && !strncmp(cmd->key, rx_buffer, keylen))
        {
            rx_pos = keylen;
            found = true;
            cmd->cb();
            break;
        }
    }
    if (!found)
    {
        log_out("Unknown command \"%s\"", rx_buffer);
        log_out(LOG_SPACER);
        for(cmd_t * cmd = cmds; cmd->key; cmd++)
            log_out("%10s : %s", cmd->key, cmd->desc);
        log_out(LOG_SPACER);
    }
    rx_buffer_len = 0;
    rx_ready = false;
}


static void cmds_task(void *args __attribute((unused)))
{
    log_out("cmds_task");
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    for (;;)
    {
        gpio_toggle(LED_PORT, LED_PIN);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        cmds_process();
    }
}


void cmds_init()
{
    xTaskCreate(cmds_task, "LED", 500, NULL, configMAX_PRIORITIES-2, &h_blinky);
}
