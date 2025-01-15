#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>

#include <driver/uart.h>

#include <osm/core/cmd.h>
#include <osm/core/log.h>
#include "pinmap.h"
#include <osm/core/ring.h>
#include <osm/core/uart_rings.h>
#include <osm/core/uarts.h>
#include <osm/core/sleep.h>
#include "platform_model.h"
#include "mqtt.h"
#include <osm/core/common.h>

static osm_uart_channel_t uart_channels[OSM_UART_CHANNELS_COUNT] = UART_CHANNELS;

static QueueHandle_t uart_queues[OSM_UART_CHANNELS_COUNT];

static TaskHandle_t uart_task_handles[OSM_UART_CHANNELS_COUNT];

static void uart_event_task(void *arg)
{
    unsigned uart = (uintptr_t)arg;
    osm_uart_channel_t * channel = &uart_channels[uart];
    QueueHandle_t * uart_queue = &uart_queues[uart];
    char tmp[128];

    while (channel->enabled)
    {
        uart_event_t event;
        if(xQueueReceive(*uart_queue, (void * )&event, (TickType_t)portMAX_DELAY))
        {
            if (event.type == UART_DATA)
            {
                int read = uart_read_bytes(channel->uart, tmp, OSM_MIN(event.size,sizeof(tmp)), portMAX_DELAY);
                for(int i=0; i < read; i++)
                {
                    char c = tmp[i];
                    osm_uart_ring_in(uart, &c, 1);
                    if (c == '\n' || c == '\r')
                    {
                        osm_sleep_debug("Waking up.");
                        osm_sleep_exit_sleep_mode();
                    }
                }
            }
        }
    }
}



static void uart_setup(unsigned uart)
{
    osm_uart_channel_t * channel = &uart_channels[uart];
    const int uart_buffer_size = 2 * 1024;

    ESP_ERROR_CHECK(uart_driver_install(channel->uart, uart_buffer_size, uart_buffer_size, 20, &uart_queues[uart], 0));
    ESP_ERROR_CHECK(uart_param_config(channel->uart, &channel->config));

    uart_set_pin(channel->uart, channel->tx_pin, channel->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    channel->enabled = 1;

    xTaskCreate(uart_event_task, "uart_event_task", 2048, (void*)(uintptr_t)uart, 12, &uart_task_handles[uart]);
}


void osm_uart_enable(unsigned uart, bool enable)
{
    if (uart >= OSM_UART_CHANNELS_COUNT || !uart)
        return;

    osm_uart_debug(uart, "%s", (enable)?"Enable":"Disable");

    osm_uart_channel_t * channel = &uart_channels[uart];

    if (!enable)
    {
        uart_driver_delete(channel->uart);
        channel->enabled = 0;
    }
    else uart_setup(uart);
}


static osm_osm_uart_parity_t _osm_uart_parity_get(uart_parity_t parity)
{
    switch(parity)
    {
        case UART_PARITY_EVEN: return osm_uart_parity_even;
        case UART_PARITY_ODD: return osm_uart_parity_odd;
        default: break;
    }

    return osm_uart_parity_none;
}


static osm_osm_uart_stop_bits_t _osm_uart_stop_bits_get(uart_stop_bits_t stop)
{
    switch(stop)
    {
        case UART_STOP_BITS_2: return osm_uart_stop_bits_2;
        case UART_STOP_BITS_1_5: return osm_uart_stop_bits_1_5;
        default: break;
    }

    return osm_uart_stop_bits_1;
}


void osm_uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_osm_uart_parity_t parity, osm_osm_uart_stop_bits_t stop, osm_cmd_ctx_t * ctx)
{
    if (uart >= OSM_UART_CHANNELS_COUNT || !uart)
        return;

    osm_uart_channel_t * channel = &uart_channels[uart];

    if (databits < 5)
    {
        osm_cmd_ctx_error(ctx, "Invalid low UART databits, using 7");
        databits = 5;
    }
    else if (databits > 8)
    {
        osm_cmd_ctx_error(ctx, "Invalid high UART databits, using 9");
        databits = 8;
    }
    channel->config.baud_rate = speed;
    channel->config.data_bits = (uart_word_length_t)(databits - 5);

    switch(parity)
    {
        case osm_uart_parity_even:
            channel->config.parity = UART_PARITY_EVEN;
            break;
        case osm_uart_parity_odd:
            channel->config.parity = UART_PARITY_ODD;
            break;
        default:
            channel->config.parity = UART_PARITY_DISABLE;
            break;
    }

    switch(stop)
    {
        case osm_uart_stop_bits_2:
            channel->config.stop_bits = UART_STOP_BITS_2;
            break;
        case osm_uart_stop_bits_1_5:
            channel->config.stop_bits = UART_STOP_BITS_1_5;
            break;
        default:
            channel->config.stop_bits = UART_STOP_BITS_1;
            break;
    }

    uart_param_config(channel->uart, &channel->config);
    osm_uart_debug(uart, "%u %"PRIu8"%c%s",
            (unsigned)channel->config.baud_rate,
            databits,
            osm_uart_parity_as_char(_osm_uart_parity_get(channel->config.parity)),
            osm_uart_stop_bits_as_str(_osm_uart_stop_bits_get(channel->config.stop_bits)));
}


bool osm_uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_osm_uart_parity_t * parity, osm_osm_uart_stop_bits_t * stop)
{
    if (uart >= OSM_UART_CHANNELS_COUNT )
        return false;

    const osm_uart_channel_t * channel = &uart_channels[uart];

    if (speed)
        *speed = channel->config.baud_rate;

    if (databits)
        *databits = ((uint8_t)channel->config.data_bits) + 5;

    if (parity)
        *parity = _osm_uart_parity_get(channel->config.parity);

    if (stop)
        *stop = _osm_uart_stop_bits_get(channel->config.stop_bits);

    return true;
}


bool osm_uart_resetup_str(unsigned uart, char * str, osm_cmd_ctx_t * ctx)
{
    uint32_t         speed;
    uint8_t          databits;
    osm_osm_uart_parity_t    parity;
    osm_osm_uart_stop_bits_t stop;

    if (uart >= OSM_UART_CHANNELS_COUNT )
    {
        osm_cmd_ctx_error(ctx, "INVALID UART GIVEN");
        return false;
    }

    if (!osm_decompose_uart_str(str, &speed, &databits, &parity, &stop))
        return false;

    osm_uart_resetup(uart, speed, databits, parity, stop, ctx);
    return true;
}




void osm_uarts_setup(void)
{
    osm_model_uarts_setup();

    for(unsigned n = 0; n < OSM_UART_CHANNELS_COUNT; n++)
        uart_setup(n);
}

bool osm_uart_is_tx_empty(unsigned uart)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return false;

    return (uart_wait_tx_done(uart_channels[uart].uart, 0) == ESP_OK);
}


void osm_uart_blocking(unsigned uart, const char *data, unsigned size)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return;

    const osm_uart_channel_t * channel = &uart_channels[uart];

    while(size)
    {
        esp_err_t err = uart_wait_tx_done(channel->uart, 200);
        if(err == ESP_OK)
        {
            int sent = uart_write_bytes(channel->uart, data, size);
            if (sent >= 0)
            {
                size -= sent;
                data += sent;
            }
            else if (sent < 0)
            {
                osm_uart_debug(uart, "Error writing to UART");
            }
        }
        else
        {
            osm_uart_debug(uart, "Trouble %s writing to the UART.", esp_err_to_name(err));
        }
    }
}


unsigned osm_uart_dma_out(unsigned uart, char *data, unsigned size)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return 0;

    const osm_uart_channel_t * channel = &uart_channels[uart];

    if (!channel->enabled)
        return size; /* Drop the data */

    if (!osm_uart_is_tx_empty(uart))
        return 0;

    if (uart == CMD_UART)
        osm_mqtt_uart_forward(data, size);

    int sent = uart_tx_chars(uart_channels[uart].uart, data, size);
    if (sent <= 0)
        return 0;
    return sent;
}


