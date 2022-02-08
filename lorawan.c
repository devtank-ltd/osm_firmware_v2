#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "uart_rings.h"
#include "uarts.h"
#include "config.h"
#include "lorawan.h"
#include "log.h"
#include "cmd.h"
#include "measurements.h"
#include "persist_config.h"
#include "sys_time.h"
#include "timers.h"

#pragma GCC diagnostic ignored "-Wstack-usage="

#define LW_BUFFER_SIZE          512
#define LW_MESSAGE_DELAY_MS     30
#define LW_CONFIG_TIMEOUT_S     30
#define LW_RESET_GPIO_MS        10

#define LW_SETTING_JOIN_MODE_OTAA       0
#define LW_SETTING_JOIN_MODE_ABP        1
#define LW_SETTING_CLASS_A              0
#define LW_SETTING_CLASS_C              2
#define LW_SETTING_UNCONFIRM            0
#define LW_SETTING_CONFIRM              1


#define LW_CONFIG_JOIN_MODE        STR(LW_SETTING_JOIN_MODE_OTAA)
#define LW_CONFIG_CLASS            STR(LW_SETTING_CLASS_C)
#define LW_CONFIG_REGION           "EU868"
#define LW_CONFIG_CONFIRM_TYPE     STR(LW_SETTING_CONFIRM)
// TODO: Dev EUI and App Key should not be baked in.
#define LW_CONFIG_DEV_EUI          "118f875d6994bbfd"
// For Chirpstack OTAA the App EUI must match the Dev EUI
#define LW_CONFIG_APP_EUI          LW_CONFIG_DEV_EUI
#define LW_CONFIG_APP_KEY          "d9597152b1293bb9c0e220cd04fc973c"

#define LW_ID_CMD       0x01434d44


typedef struct
{
    union
    {
        struct
        {
            int16_t port;
            int16_t rssi;
            int16_t snr;
            int16_t datalen;
        };
        int16_t raw[4];
    } header;
    char* data;
} lw_payload_t;


typedef struct
{
    int8_t hex_arr[MEASUREMENTS_HEX_ARRAY_SIZE];
    uint16_t len;
} lw_lora_message_t;


static port_n_pins_t lw_reset_gpio = { GPIOC, GPIO8 };
static char lw_out_buffer[LW_BUFFER_SIZE] = {0};
static char lw_leftover[LW_BUFFER_SIZE] = {0};
static lw_lora_message_t lw_message_backup = {{0}, 0};
volatile bool ready = true;
static uint8_t lw_port = 0;
static bool lw_connected = false;
static uint32_t lw_sent_stm32 = 0;


static void lw_reset_chip(void)
{
    gpio_clear(lw_reset_gpio.port, lw_reset_gpio.pins);
    timer_delay_us_64(LW_RESET_GPIO_MS * 1000);
    gpio_set(lw_reset_gpio.port, lw_reset_gpio.pins);
}


static void lw_update_sent_stm32(void)
{
    lw_sent_stm32 = since_boot_ms;
}


static void lw_write_to_uart(char* fmt, va_list args)
{
    size_t len;

    vsnprintf(lw_out_buffer, LW_BUFFER_SIZE, fmt, args);
    lw_debug("<< %s", lw_out_buffer);
    len = strlen(lw_out_buffer);
    lw_out_buffer[len] = '\r';
    lw_out_buffer[len+1] = '\n';
    uart_ring_out(LW_UART, lw_out_buffer, strlen(lw_out_buffer));
    memset(lw_out_buffer, 0, LW_BUFFER_SIZE);
    lw_update_sent_stm32();
}


static void lw_write(char* cmd_fmt, ...)
{
    va_list args;

    va_start(args, cmd_fmt);
    lw_write_to_uart(cmd_fmt, args);
    va_end(args);
}


static void lw_set_config(const char* config_fmt, ...)
{
    va_list args;

    va_start(args, config_fmt);
    lw_write_to_uart((char*)config_fmt, args);
    va_end(args);
}


typedef enum
{
    LW_STATE_PREINIT       ,
    LW_STATE_INIT          ,
    LW_STATE_IDLE          ,
    LW_STATE_WAITING_RSP   ,
    LW_STATE_WAITING_LW_ACK,
    LW_STATE_WAITING_LW_RSP,
} lw_states_t;


typedef void (*lw_response_cb)(void);


typedef struct
{
    lw_states_t state;
    union
    {
        unsigned init_step;
        lw_response_cb response_cb;
    } data;
} lw_state_machine_t;


lw_state_machine_t lw_state_machine = {LW_STATE_INIT, .data={.init_step = 0}};


static char lw_dev_eui[LW_DEV_EUI_LEN + 1];
static char lw_app_key[LW_APP_KEY_LEN + 1];

typedef char lw_msg_buf_t[64];

static lw_msg_buf_t init_msgs[] = { "at+set_config=lora:default_parameters",
                         "at+set_config=lora:join_mode:"LW_CONFIG_JOIN_MODE,
                         "at+set_config=lora:class:"LW_CONFIG_CLASS,
                         "at+set_config=lora:region:"LW_CONFIG_REGION,
                         "at+set_config=lora:confirm:"LW_CONFIG_CONFIRM_TYPE,
                         "at+set_config=lora:dev_eui:"LW_CONFIG_DEV_EUI,
                         "at+set_config=lora:app_eui:"LW_CONFIG_APP_EUI,
                         "at+set_config=lora:app_key:"LW_CONFIG_APP_KEY,
                         "at+set_config=device:restart",
                         "at+join" };


static void lw_reset_gpio_init(void)
{
    rcc_periph_clock_enable(PORT_TO_RCC(lw_reset_gpio.port));
    gpio_mode_setup(lw_reset_gpio.port,
                    GPIO_MODE_OUTPUT,
                    GPIO_PUPD_NONE,
                    lw_reset_gpio.pins);
    gpio_set(lw_reset_gpio.port, lw_reset_gpio.pins);
}


void lorawan_init(void)
{
    lw_reset_gpio_init();
    if (lw_state_machine.state != LW_STATE_INIT || lw_state_machine.data.init_step != 0)
    {
        log_error("LW not expected in init state.");
        return;
    }

    if (persist_get_lw_dev_eui(lw_dev_eui) && persist_get_lw_app_key(lw_app_key))
    {
        snprintf(init_msgs[5], sizeof(lw_msg_buf_t), "at+set_config=lora:dev_eui:%s", lw_dev_eui);
        snprintf(init_msgs[6], sizeof(lw_msg_buf_t), "at+set_config=lora:app_eui:%s", lw_dev_eui);
        snprintf(init_msgs[7], sizeof(lw_msg_buf_t), "at+set_config=lora:app_key:%s", lw_app_key);
    }

    lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
}


void lw_reconnect(void)
{
    lw_debug("restarted LoRaWAN connection");
    lw_state_machine.state = LW_STATE_INIT;
    lw_state_machine.data.init_step = 0;
    lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
}


enum
{
    LW_RECV_ACK,
    LW_RECV_DATA,
    LW_RECV_ERR_NOT_START,
    LW_RECV_ERR_BAD_FMT,
    LW_RECV_ERR_UNFIN,
};


static bool lw_msg_is_unsoclitied(char* message);
static bool lw_msg_is_ok(char* message);
static bool lw_msg_is_error(char* message);
static bool lw_msg_is_ack(char* message);
static void lw_error_handle(char* message);
static void lw_handle_unsol(char* message);


void lw_process(char* message)
{
    lw_update_sent_stm32();
    if (lw_state_machine.state == LW_STATE_WAITING_RSP)
    {
        if (lw_msg_is_ok(message))
        {
            lw_debug(">> (UNSOL) %s", message);
            lw_state_machine.state = LW_STATE_IDLE;
            return;
        }
        /*else error?*/
    }
    else if (lw_state_machine.state == LW_STATE_WAITING_LW_ACK)
    {
        if (lw_msg_is_ok(message))
        {
            return;
        }
        if (lw_msg_is_error(message))
        {
            lw_error_handle(message);
            return; /* Logged in check */
        }
        if (lw_msg_is_ack(message))
        {
            lw_state_machine.state = LW_STATE_IDLE;
            on_lw_sent_ack(true);
            return;
        }

        /*ERROR*/
    }
    else if (lw_state_machine.state == LW_STATE_WAITING_LW_RSP)
    {
        if (lw_msg_is_ok(message))
        {
            lw_state_machine.state = LW_STATE_IDLE;
            return;
        }
        if (lw_msg_is_error(message))
        {
            lw_error_handle(message);
            return; /* Logged in check */
        }

        if (lw_state_machine.data.response_cb)
        {
            lw_state_machine.data.response_cb();
            lw_state_machine.data.response_cb = NULL;
        }
        lw_state_machine.state = LW_STATE_IDLE;
    }
    else if ((lw_state_machine.state == LW_STATE_INIT) && (lw_state_machine.data.init_step < 9))
    {
        if (lw_msg_is_ok(message))
        {
            lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
        }
        else
        {
            lw_debug("Setting not OKed. Retrying.");
            lw_set_config(init_msgs[--lw_state_machine.data.init_step]);
        }
        timer_delay_us_64(LW_MESSAGE_DELAY_MS * 1000);
    }
    else if ((lw_state_machine.state == LW_STATE_INIT) && (lw_state_machine.data.init_step == 9)) /*Restart*/
    {
        if (strstr(message, "UART1") == message ||
            strstr(message, "Current work") == message)
        {
            lw_debug("Valid Init 7 response line");
        }
        else if (strcmp(message, "Initialization OK") == 0)
        {
            lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
        }
        else
        {
             /*HANDLE ERROR!*/
        }
    }
    else if ((lw_state_machine.state == LW_STATE_INIT) && (lw_state_machine.data.init_step == 10)) /*Join*/
    {
        if (strcmp(message, "OK Join Success") == 0)
        {
            lw_state_machine.state = LW_STATE_IDLE;
            lw_connected = true;
            lw_send_alive();
        }
        else if (lw_msg_is_error(message))
        {
            lw_error_handle(message);
            /*else error*/
        }
    }
    else if (lw_state_machine.state != LW_STATE_WAITING_LW_ACK)
    {
        if (lw_msg_is_unsoclitied(message))
        {
            /*Done?*/
            lw_debug(">> (UNSOL) %s", message);
            lw_handle_unsol(message);
            return;
        }
    }
    else
    {
        log_error("Unexpected data for LW state.");
    }
}


static uint8_t lw_parse_recv(char* message, lw_payload_t* payload)
{
    // at+recv=PORT,RSSI,SNR,DATALEN:DATA
    char recv_msg[] = "at+recv=";
    char* pos = NULL;
    char* next_pos = NULL;
    char proc_str[LW_BUFFER_SIZE] = "";

    if (strncmp(message, recv_msg, strlen(recv_msg)) != 0)
    {
        if (strncmp(lw_leftover, recv_msg, strlen(recv_msg)) != 0)
        {
            return LW_RECV_ERR_NOT_START;
        }
        strncat(proc_str, lw_leftover, strlen(lw_leftover));
        strncat(proc_str, message, strlen(message));
    }
    else
    {
        memset(lw_leftover, 0, LW_BUFFER_SIZE);
        strncpy(proc_str, message, strlen(message));
    }

    pos = proc_str + strlen(recv_msg);
    for (int i = 0; i < 3; i++)
    {
        payload->header.raw[i] = strtol(pos, &next_pos, 10);
        if ((*next_pos) != ',')
        {
            return LW_RECV_ERR_BAD_FMT;
        }
        pos = next_pos + 1;
    }
    payload->header.datalen = strtol(pos, &next_pos, 10);
    if (*next_pos == '\0')
    {
        payload->data = NULL;
        if (payload->header.datalen == 0)
        {
            return LW_RECV_ACK;
        }
        return LW_RECV_ERR_BAD_FMT;
    }
    if (*next_pos == ':')
    {
        payload->data = next_pos + 1;
        int size_diff = strlen(payload->data)/2 - (int)payload->header.datalen;
        if (size_diff < 0)
        {
            strncat(lw_leftover, proc_str, strlen(proc_str));
            return LW_RECV_ERR_UNFIN;
        }
        if (size_diff > 0)
        {
            return LW_RECV_ERR_BAD_FMT;
        }
        return LW_RECV_DATA;
    }
    return LW_RECV_ERR_BAD_FMT;
}


static bool lw_msg_is_unsoclitied(char* message)
{
    lw_payload_t payload;
    if (!lw_parse_recv(message, &payload))
    {
        return false;
    }
    if (payload.header.datalen == 0)
    {
        // Ack?
        return false;
    }
    return true;
}


static bool lw_msg_is_ok(char* message)
{
    return (bool)(strncmp(message, "OK ", strlen(message)) == 0);
}


static bool lw_msg_is_error(char* message)
{
    char err_msg[] = "ERROR: ";
    return (bool)(strncmp(message, err_msg, strlen(err_msg)) == 0);
}


static bool lw_msg_is_ack(char* message)
{
    lw_payload_t payload;
    return (lw_parse_recv(message, &payload) == LW_RECV_ACK);
}

enum
{
    LW_ERROR_TX_ERROR      =  5,  // more than 256 bytes?
    LW_ERROR_NOT_CONNECTED =  86, // reconnect
    LW_ERROR_PACKET_SIZE   =  87, // throw away message
    LW_ERROR_TIMEOUT_RX1   =  95, // resend
    LW_ERROR_TIMEOUT_RX2   =  96, // resend
    LW_ERROR_RECV_RX1      =  97, // to be decided
    LW_ERROR_RECV_RX2      =  98, // to be decided
    LW_ERROR_JOIN_FAIL     =  99, // reconnect
    LW_ERROR_DUP_DOWNLINK  = 100, // probably nothing
    LW_ERROR_PAYLOAD_SIZE  = 101, // throw away message
};


static void lw_resend_message(void);


static void lw_error_handle(char* message)
{
    char* pos = NULL;
    char* next_pos = NULL;
    char err_msg[] = "ERROR: ";
    uint8_t err_no = 0;

    pos = message + strlen(err_msg);
    err_no = strtol(pos, &next_pos, 10);
    switch (err_no)
    {
        case LW_ERROR_TX_ERROR:
            lw_reconnect();
            lw_debug("Error sending data (too big?)");
            break;
        case LW_ERROR_NOT_CONNECTED:
            lw_reconnect();
            lw_debug("Not connected to a network.");
            break;
        case LW_ERROR_JOIN_FAIL:
            lw_reconnect();
            lw_debug("Failed to join network.");
            break;
        case LW_ERROR_TIMEOUT_RX1:
            lw_resend_message();
            lw_debug("RX1 Window timed out.");
            break;
        case LW_ERROR_TIMEOUT_RX2:
            lw_resend_message();
            lw_debug("RX1 Window timed out.");
            break;
        default:
            lw_debug("Error: %"PRIu8, err_no);
            lw_state_machine.state = LW_STATE_IDLE;
            break;
    }
}


static void lw_handle_unsol(char* message)
{
    lw_payload_t incoming_pl;
    if (lw_parse_recv(message, &incoming_pl) != LW_RECV_DATA)
    {
        return;
    }

    char* p = incoming_pl.data;

    char pl_id_s[8] = "";
    strncpy(pl_id_s, p, 8);
    uint32_t pl_id = strtoul(pl_id_s, NULL, 16);

    p += 8;

    char cmd_ascii[LW_BUFFER_SIZE] = "";
    char val_str[3] = "";
    uint8_t val;
    for (size_t i = 0; i < strlen(p) / 2; i++)
    {
        strncpy(val_str, p + 2*i, 2);
        val = strtoul(val_str, NULL, 16);
        strncat(cmd_ascii, (char* )&val, 1);
    }

    switch (pl_id)
    {
        case LW_ID_CMD:
            cmds_process(cmd_ascii, strlen(cmd_ascii));
            break;
        default:
            break;
    }
}


void lw_send(int8_t* hex_arr, uint16_t arr_len)
{
    char header_str[LW_HEADER_SIZE + 1] = {0};
    char hex_str[3] = {0};
    if (lw_state_machine.state == LW_STATE_IDLE)
    {
        unsigned expected = lw_send_size(arr_len);
        lw_port++;
        if (lw_port > 223)
        {
            lw_port = 0;
        }
        unsigned header_size = snprintf(header_str, sizeof(header_str), "at+send=lora:%"PRIu8":", lw_port);
        unsigned sent = 0;
        sent+= uart_ring_out(LW_UART, header_str, header_size);
        uart_ring_out(CMD_UART, header_str, header_size);
        for (uint16_t i = 0; i < arr_len; i++)
        {
            snprintf(hex_str, 3, "%02"PRIx8, hex_arr[i]);
            sent+= uart_ring_out(LW_UART, hex_str, 2);
            uart_ring_out(CMD_UART, hex_str, 2);
        }
        sent+= uart_ring_out(LW_UART, "\r\n", 2);
        uart_ring_out(CMD_UART, "\r\n", 2);
        lw_update_sent_stm32();

        if (sent != expected)
            log_error("Failed to send all bytes over LoRaWAN (%u != %u)", sent, expected);
        else
            lw_debug("Sent %u bytes", sent);

        memcpy(lw_message_backup.hex_arr, hex_arr, MEASUREMENTS_HEX_ARRAY_SIZE);
        lw_message_backup.len = arr_len;
        lw_state_machine.state = LW_STATE_WAITING_LW_ACK;
    }
    else lw_debug("Incorrect state to send : %u", (unsigned)lw_state_machine.state);
}


bool lw_send_ready(void)
{
    return lw_state_machine.state == LW_STATE_IDLE;
}


unsigned lw_send_size(uint16_t arr_len)
{
    unsigned next_lw_port = lw_port + 1;
    if (next_lw_port > 223)
        next_lw_port = 0;

    unsigned size = (LW_HEADER_SIZE + (arr_len * 2) + LW_TAIL_SIZE);

    if (next_lw_port < 99)
    {
        size--;
        if (next_lw_port < 9)
            size--;
    }

    return size;
}


bool lw_get_connected(void)
{
    return lw_connected;
}


void lw_send_str(char* str)
{
    lw_port++;
    if (lw_port > 223)
    {
        lw_port = 0;
    }
    lw_write("at+send=lora:%u:%s", lw_port, str);
    lw_state_machine.state = LW_STATE_WAITING_LW_ACK;
}


void lw_send_alive(void)
{
    lw_send_str("");
}


static void lw_resend_message(void)
{
    if (lw_state_machine.state == LW_STATE_WAITING_LW_ACK)
        lw_state_machine.state = LW_STATE_IDLE;
    lw_send(lw_message_backup.hex_arr, lw_message_backup.len);
}


void lw_loop_iteration(void)
{
    if (( lw_state_machine.state != LW_STATE_IDLE ) && (since_boot_delta(since_boot_ms, lw_sent_stm32) > LW_CONFIG_TIMEOUT_S * 1000))
    {
        if (lw_state_machine.state == LW_STATE_WAITING_LW_ACK)
        {
            on_lw_sent_ack(false);
        }
        lw_reset_chip();
        lw_state_machine.state = LW_STATE_INIT;
        lw_state_machine.data.init_step = 0;
        lorawan_init();
    }
}
