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



typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(void);
} cmd_t;


static void uart_broadcast_cb();
static void debug_cb();
static void pps_cb();
static void adc_cb();
static void input_cb();
static void output_cb();
static void count_cb();


static cmd_t cmds[] = {
    { "uarts",  "Broad message to uarts.", uart_broadcast_cb},
    { "debug",  "Toggle debug",            debug_cb},
    { "ppss",   "Print all pulse info.",   pulsecount_log},
    { "pps",    "Print pulse info.",       pps_cb},
    { "adc",    "Print ADC.",              adc_cb},
    { "adcs",   "Print all ADCs.",         adcs_log},
    { "inputs", "Print all inputs.",       inputs_log},
    { "outputs","Print all outputs.",      outputs_log},
    { "input",  "Print input.",            input_cb},
    { "output", "Get/set output on/off.",  output_cb},
    { "count",  "Counts of controls.",     count_cb},
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
    unsigned pps = strtoul(rx_buffer + rx_pos, NULL, 10);

    pulsecount_pps_log(pps);
}


void adc_cb()
{
    unsigned adc = strtoul(rx_buffer + rx_pos, NULL, 10);
    adcs_adc_log(adc);
}


void input_cb()
{
    unsigned input = strtoul(rx_buffer + rx_pos, NULL, 10);
    inputs_input_log(input);
}


void output_cb()
{
    char * pos = NULL;
    unsigned output = strtoul(rx_buffer + rx_pos, &pos, 10);

    while(*pos == ' ')
        pos++;

    if (*pos)
    {
        bool on_off = (strtoul(pos, NULL, 10))?true:false;

        log_out("Set output %02u to %s", output, (on_off)?"ON":"OFF");
        outputs_set(output, on_off);
    }
    else output_output_log(output);
}


void count_cb()
{
    log_out("PPS     : %u", pulsecount_get_count());
    log_out("Inputs  : %u", inputs_get_count());
    log_out("Outputs : %u", outputs_get_count());
    log_out("ADCs    : %u", adcs_get_count());
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
    log_out(LOG_START_SPACER);
    for(cmd_t * cmd = cmds; cmd->key; cmd++)
    {
        unsigned keylen = strlen(cmd->key);
        if(rx_buffer_len >= keylen &&
           !strncmp(cmd->key, rx_buffer, keylen) &&
           (rx_buffer[keylen] == '\0' || rx_buffer[keylen] == ' '))
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
    }
    log_out(LOG_END_SPACER);
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
