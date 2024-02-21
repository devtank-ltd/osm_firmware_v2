#include <stdint.h>
#include <string.h>

#ifdef STM32L4
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#endif

#include "base_types.h"
#include "uarts.h"
#include "io.h"
#include "common.h"
#include "platform.h"


#define PASSTHROUGH_TIMEOUT_MS                  10000


static const uart_channel_t _passthrough_default_uarts[] = UART_CHANNELS;

static struct
{
    const uart_channel_t*       default_cmd_uart;
    const uart_channel_t*       default_comms_uart;
    port_n_pins_t               cmd_rx_ports_n_pins;
    port_n_pins_t               cmd_tx_ports_n_pins;
    port_n_pins_t               comms_rx_ports_n_pins;
    port_n_pins_t               comms_tx_ports_n_pins;
    uint32_t                    last_msg_time;
    bool                        prev_cmd_rx;
    bool                        prev_comms_rx;
    uart_channel_t              prev_cmd_uart;
    uart_channel_t              prev_comms_uart;
}  _passthrough_ctx =
{
    .default_cmd_uart   = NULL,
    .default_comms_uart = NULL,
    .last_msg_time      = 0,
    .prev_cmd_rx        = false,
    .prev_comms_rx      = false,
};


void passthrough_init(void)
{
    _passthrough_ctx.default_cmd_uart = &_passthrough_default_uarts[CMD_UART];
    _passthrough_ctx.default_comms_uart = &_passthrough_default_uarts[COMMS_UART];

    _passthrough_ctx.cmd_rx_ports_n_pins.port = _passthrough_default_uarts[CMD_UART].gpioport;
    _passthrough_ctx.cmd_rx_ports_n_pins.pins = GPIO3;

    _passthrough_ctx.cmd_tx_ports_n_pins.port = _passthrough_default_uarts[CMD_UART].gpioport;
    _passthrough_ctx.cmd_tx_ports_n_pins.pins = GPIO2;

    _passthrough_ctx.comms_rx_ports_n_pins.port = _passthrough_default_uarts[COMMS_UART].gpioport;
    _passthrough_ctx.comms_rx_ports_n_pins.pins = GPIO5;

    _passthrough_ctx.comms_tx_ports_n_pins.port = _passthrough_default_uarts[COMMS_UART].gpioport;
    _passthrough_ctx.comms_tx_ports_n_pins.pins = GPIO4;
}


static void _passthrough_setup(void)
{
    unsigned speed;
    uint8_t databits;
    osm_uart_parity_t parity;
    osm_uart_stop_bits_t stop_bits;
    uart_channel_t* cmd_uart = &_passthrough_ctx.prev_cmd_uart;
    uart_channel_t* comms_uart = &_passthrough_ctx.prev_comms_uart;

    if (uart_get_setup(CMD_UART, &speed, &databits, &parity, &stop_bits))
    {
        cmd_uart->baud       = speed;
        cmd_uart->databits   = databits;
        cmd_uart->parity     = parity;
        cmd_uart->stop       = stop_bits;
    }
    else
    {
        memcpy(cmd_uart, _passthrough_ctx.default_cmd_uart, sizeof(uart_channel_t));
    }

    if (uart_get_setup(COMMS_UART, &speed, &databits, &parity, &stop_bits))
    {
        comms_uart->baud       = speed;
        comms_uart->databits   = databits;
        comms_uart->parity     = parity;
        comms_uart->stop       = stop_bits;
    }
    else
    {
        memcpy(comms_uart, _passthrough_ctx.default_comms_uart, sizeof(uart_channel_t));
    }

    platform_gpio_setup(&_passthrough_ctx.cmd_rx_ports_n_pins, true, IO_PUPD_NONE);
    platform_gpio_setup(&_passthrough_ctx.cmd_tx_ports_n_pins, false, IO_PUPD_NONE);
    platform_gpio_setup(&_passthrough_ctx.comms_rx_ports_n_pins, true, IO_PUPD_NONE);
    platform_gpio_setup(&_passthrough_ctx.comms_tx_ports_n_pins, false, IO_PUPD_NONE);
}


static void _passthrough_exit(void)
{
    uart_channel_t* cmd = &_passthrough_ctx.prev_cmd_uart;
    uart_channel_t* comms = &_passthrough_ctx.prev_comms_uart;
    uart_resetup(CMD_UART, cmd->baud, cmd->databits, cmd->parity, cmd->stop);
    uart_resetup(COMMS_UART, comms->baud, comms->databits, comms->parity, comms->stop);
}


static bool _passthrough_loop_iteration(void* userdata)
{
    const bool cmd_rx = platform_gpio_get(&_passthrough_ctx.cmd_rx_ports_n_pins);
    const bool comms_rx = platform_gpio_get(&_passthrough_ctx.comms_rx_ports_n_pins);

    uint32_t now = get_since_boot_ms();
    if (cmd_rx   != _passthrough_ctx.prev_cmd_rx)
    {
        platform_gpio_set(&_passthrough_ctx.comms_tx_ports_n_pins, cmd_rx);
        _passthrough_ctx.prev_cmd_rx = cmd_rx;
        _passthrough_ctx.last_msg_time = now;
    }
    if (comms_rx != _passthrough_ctx.prev_comms_rx)
    {
        platform_gpio_set(&_passthrough_ctx.cmd_tx_ports_n_pins, comms_rx);
        _passthrough_ctx.prev_comms_rx = comms_rx;
        _passthrough_ctx.last_msg_time = now;
    }

    return since_boot_delta(get_since_boot_ms(), _passthrough_ctx.last_msg_time) > PASSTHROUGH_TIMEOUT_MS;
}


static void _passthrough_enter(void)
{
    _passthrough_ctx.last_msg_time = get_since_boot_ms();

    _passthrough_setup();

    bool done = false;
    while (!done)
    {
        done = main_loop_iterate_for(0xFFFFFFFF, _passthrough_loop_iteration, NULL);
    }

    _passthrough_exit();
}


static command_response_t _passthrough_enter_cb(char* args)
{
    _passthrough_enter();
    return COMMAND_RESP_OK;
}


struct cmd_link_t* passthrough_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "passthrough",    "Enter passthrough mode",       _passthrough_enter_cb       , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
