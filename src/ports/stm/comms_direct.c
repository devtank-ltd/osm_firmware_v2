#include <stdint.h>
#include <string.h>

#include <osm/core/base_types.h>
#include <osm/core/uarts.h>
#include <osm/core/io.h>
#include <osm/core/common.h>
#include <osm/core/platform.h>
#include <osm/core/uart_rings.h>
#include <osm/core/log.h>


#define COMMS_DIRECT_TIMEOUT_MS                  3000


static const uart_channel_t _comms_direct_default_uarts[] = UART_CHANNELS;

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
    bool                        begin;
}  _comms_direct_ctx =
{
    .default_cmd_uart   = NULL,
    .default_comms_uart = NULL,
    .last_msg_time      = 0,
    .prev_cmd_rx        = false,
    .prev_comms_rx      = false,
    .begin              = false,
};


void comms_direct_init(void)
{
    const uart_channel_t* cmd_uart = &_comms_direct_default_uarts[CMD_UART];
    const uart_channel_t* comms_uart = &_comms_direct_default_uarts[COMMS_UART];
    _comms_direct_ctx.default_cmd_uart = cmd_uart;
    _comms_direct_ctx.default_comms_uart = comms_uart;

    _comms_direct_ctx.cmd_rx_ports_n_pins.port = cmd_uart->gpioport;
    _comms_direct_ctx.cmd_rx_ports_n_pins.pins = cmd_uart->rx_pin;

    _comms_direct_ctx.cmd_tx_ports_n_pins.port = cmd_uart->gpioport;
    _comms_direct_ctx.cmd_tx_ports_n_pins.pins = cmd_uart->tx_pin;

    _comms_direct_ctx.comms_rx_ports_n_pins.port = comms_uart->gpioport;
    _comms_direct_ctx.comms_rx_ports_n_pins.pins = comms_uart->rx_pin;

    _comms_direct_ctx.comms_tx_ports_n_pins.port = comms_uart->gpioport;
    _comms_direct_ctx.comms_tx_ports_n_pins.pins = comms_uart->tx_pin;
}


static void _comms_direct_setup(void)
{
    unsigned speed;
    uint8_t databits;
    osm_uart_parity_t parity;
    osm_uart_stop_bits_t stop_bits;
    uart_channel_t* cmd_uart = &_comms_direct_ctx.prev_cmd_uart;
    uart_channel_t* comms_uart = &_comms_direct_ctx.prev_comms_uart;
    if (uart_get_setup(CMD_UART, &speed, &databits, &parity, &stop_bits))
    {
        cmd_uart->baud       = speed;
        cmd_uart->databits   = databits;
        cmd_uart->parity     = parity;
        cmd_uart->stop       = stop_bits;
    }
    else
    {
        memcpy(cmd_uart, _comms_direct_ctx.default_cmd_uart, sizeof(uart_channel_t));
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
        memcpy(comms_uart, _comms_direct_ctx.default_comms_uart, sizeof(uart_channel_t));
    }
    uart_enable(CMD_UART, false);
    uart_enable(COMMS_UART, false);

    platform_gpio_setup(&_comms_direct_ctx.cmd_rx_ports_n_pins, true, IO_PUPD_NONE);
    platform_gpio_setup(&_comms_direct_ctx.cmd_tx_ports_n_pins, false, IO_PUPD_UP);
    platform_gpio_setup(&_comms_direct_ctx.comms_rx_ports_n_pins, true, IO_PUPD_NONE);
    platform_gpio_setup(&_comms_direct_ctx.comms_tx_ports_n_pins, false, IO_PUPD_UP);
}


static void _comms_direct_exit(void)
{
    uart_channel_t* cmd = &_comms_direct_ctx.prev_cmd_uart;
    uart_channel_t* comms = &_comms_direct_ctx.prev_comms_uart;
    uart_resetup(CMD_UART, cmd->baud, cmd->databits, cmd->parity, cmd->stop, &uart_cmd_ctx);
    uart_resetup(COMMS_UART, comms->baud, comms->databits, comms->parity, comms->stop, &uart_cmd_ctx);
    uarts_setup();
    uart_enable(CMD_UART, true);
    uart_enable(COMMS_UART, true);
    uart_rings_in_wipe(CMD_UART);
    uart_rings_in_wipe(COMMS_UART);
    log_out("Exiting COMMS_DIRECT mode");
}


static bool _comms_direct_loop_iteration(void)
{
    const bool cmd_rx = platform_gpio_get(&_comms_direct_ctx.cmd_rx_ports_n_pins);
    const bool comms_rx = platform_gpio_get(&_comms_direct_ctx.comms_rx_ports_n_pins);

    uint32_t now = get_since_boot_ms();
    if (cmd_rx   != _comms_direct_ctx.prev_cmd_rx)
    {
        platform_gpio_set(&_comms_direct_ctx.comms_tx_ports_n_pins, cmd_rx);
        _comms_direct_ctx.prev_cmd_rx = cmd_rx;
        _comms_direct_ctx.last_msg_time = now;
    }
    if (comms_rx != _comms_direct_ctx.prev_comms_rx)
    {
        platform_gpio_set(&_comms_direct_ctx.cmd_tx_ports_n_pins, comms_rx);
        _comms_direct_ctx.prev_comms_rx = comms_rx;
        _comms_direct_ctx.last_msg_time = now;
    }

    return since_boot_delta(get_since_boot_ms(), _comms_direct_ctx.last_msg_time) > COMMS_DIRECT_TIMEOUT_MS;
}


static void _comms_direct_preamble(void)
{
    _comms_direct_ctx.last_msg_time = get_since_boot_ms();

    log_out("Entering COMMS_DIRECT mode");
    uart_rings_drain_all_out();

    /* As we are changing the pins for the UART, need to wait for the
     * DMA to finish writing to the UART, and then the UART to finish
     * writing to the pins. */
    bool done = false;
    while (!done)
    {
        for (unsigned i = 0; i < ARRAY_SIZE(_comms_direct_default_uarts); i++)
        {
            done = true;
            if (!usart_get_flag(_comms_direct_default_uarts[i].usart, USART_ISR_TC))
            {
                done = false;
                break;
            }
        }
    }
}


static void _comms_direct_enter(void)
{
    _comms_direct_preamble();

    _comms_direct_setup();

    bool done = false;
    while (!done)
    {
        done = _comms_direct_loop_iteration();
        platform_watchdog_reset();
    }

    _comms_direct_exit();
}


static command_response_t _comms_direct_enter_cb(char* args, cmd_ctx_t * ctx)
{
    if (ctx != &uart_cmd_ctx)
    {
        cmd_ctx_error(ctx, "Only works on Command UART.");
        return COMMAND_RESP_ERR;
    }
    _comms_direct_ctx.begin = true;
    return COMMAND_RESP_OK;
}


struct cmd_link_t* comms_direct_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_direct",    "Enter comms_direct mode",       _comms_direct_enter_cb       , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}


void comms_direct_loop_iterate(void)
{
    if (_comms_direct_ctx.begin)
    {
        _comms_direct_ctx.begin = false;
        _comms_direct_enter();
    }
}
