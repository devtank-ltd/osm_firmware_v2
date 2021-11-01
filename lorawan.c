#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>

#include "uart_rings.h"
#include "uarts.h"
#include "config.h"
#include "lorawan.h"
#include "log.h"
#pragma GCC diagnostic ignored "-Wstack-usage="

#define STR_EXPAND(tok) #tok            ///< Convert macro value to a string.
#define STR(tok) STR_EXPAND(tok)        ///< Convert macro value to a string.

#define LW_BUFFER_SIZE 64
#define LW_MESSAGE_DELAY 3000

#define LW_JOIN_MODE_OTAA   0
#define LW_JOIN_MODE_ABP    1
#define LW_JOIN_MODE        LW_JOIN_MODE_OTAA
#define LW_CLASS_A          0
#define LW_CLASS_C          2
#define LW_CLASS            LW_CLASS_C
#define LW_REGION           "EU868"
// TODO: Dev EUI and App Key should not be baked in.
#define LW_DEV_EUI          "118f875d6994bbfd"
// For Chirpstack OTAA the App EUI must match the Dev EUI
#define LW_APP_EUI          LW_DEV_EUI
#define LW_APP_KEY          "d9597152b1293bb9c0e220cd04fc973c"



#define LW_FMT__TEMPERATURE                 "%02X%02X%04X"


static char lw_out_buffer[LW_BUFFER_SIZE] = {0};
volatile bool ready = true;


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
    };
    char* data;
} lw_header_t;


static void lw_spin_us(uint32_t time_us)
{
    uint64_t num_loops = (rcc_ahb_frequency / 1e6) * time_us;
    for (uint64_t i = 0; i < num_loops; i++)
    {
        asm("nop");
    }
}


static void lw_write_to_uart(char* fmt, va_list args)
{
    size_t len;

    vsnprintf(lw_out_buffer, LW_BUFFER_SIZE, fmt, args);
    log_out("LORA << %s", lw_out_buffer);
    len = strlen(lw_out_buffer);
    lw_out_buffer[len] = '\r';
    lw_out_buffer[len+1] = '\n';
    uart_ring_out(LW_UART, lw_out_buffer, LW_BUFFER_SIZE);
    memset(lw_out_buffer, 0, LW_BUFFER_SIZE);
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

/*
static void lw_set_confirmation(bool conf)
{
    uint8_t val = 0;
    if (conf)
    {
        val = 1;
    }
    lw_set_config("at+set_config=lora:confirm:%u", val);
}*/

typedef enum 
{
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


const char init_msgs[][64] = { "at+set_config=lora:default_parameters",
                               "at+set_config=lora:join_mode:"STR(LW_JOIN_MODE),
                               "at+set_config=lora:class:"STR(LW_CLASS),
                               "at+set_config=lora:region:"LW_REGION,
                               "at+set_config=lora:dev_eui:"LW_DEV_EUI,
                               "at+set_config=lora:app_eui:"LW_APP_EUI,
                               "at+set_config=lora:app_key:"LW_APP_KEY,
                               "at+set_config=device:restart",
                               "at+join" };


bool lw_send_with_rsp(lw_packet_t* packet, lw_response_cb cb)
{
    if (lw_state_machine.state != LW_STATE_INIT)
    {
        log_error("LW not in state to send query");
        return false;
    }
    
    if (lw_send_packet(packet))
    {
        lw_state_machine.data.response_cb = cb;
        return true;
    }

    return false;
}


void lorawan_init(void)
{
    if (lw_state_machine.state != LW_STATE_INIT || lw_state_machine.data.init_step != 0)
    {
        log_error("LW not expected in init state.");
        return;
    }
    lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
}


static void lw_reconnect(void)
{
    lw_state_machine.state = LW_STATE_INIT;
    lw_state_machine.data.init_step = 0;
    lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
}


static bool lw_payload_add(lw_measurement_t* m, char* payload)
{
    char p[32] = {0};
    snprintf(p, 32*sizeof(char), m->format, m->channel, m->type_id, m->data);
    printf("%s\n", p);
    strncat(payload, p, strlen(p));
    return true;
}


bool lw_send_packet(lw_packet_t* packet)
{
    char payload[64] = {0};
    uint8_t port = 5;
    for(lw_measurement_t* m = packet->measurements; m->id; m++)
    {
        if (!lw_payload_add(m, payload))
        {
            return false;
        }
    }
    lw_write("at+send=lora:%u:%s", port, payload);
    lw_state_machine.state = LW_STATE_WAITING_LW_ACK;
    return true;
}


static bool lw_msg_is_unsoclitied(char* message);
static bool lw_msg_is_ok(char* message);
static bool lw_msg_is_error(char* message);
static bool lw_msg_is_ack(char* message);
static void lw_error_handle(char* message);


void lw_process(char* message)
{

    if (lw_state_machine.state == LW_STATE_WAITING_RSP)
    {
        if (lw_msg_is_ok(message))
        {
            lw_state_machine.state = LW_STATE_IDLE;
            log_out("HACK 2");
            return;
        }
        log_out("HACK 2.1");
        /*else error?*/
    }
    else if (lw_state_machine.state == LW_STATE_WAITING_LW_ACK)
    {
        log_out("HACK 3");
        if (lw_msg_is_ok(message))
            return;
        if (lw_msg_is_error(message))
        {
            lw_error_handle(message);
            return; /* Logged in check */
        }
        if (lw_msg_is_ack(message))
        {
            lw_state_machine.state = LW_STATE_IDLE;
            return;
        }

        /*ERROR*/
    }
    else if (lw_state_machine.state == LW_STATE_WAITING_LW_RSP)
    {
        log_out("HACK 4");
        if (lw_msg_is_ok(message))
            return;
        if (lw_msg_is_error(message))
            return; /* Logged in check */
        
        if (lw_state_machine.data.response_cb)
        {
            lw_state_machine.data.response_cb();
            lw_state_machine.data.response_cb = NULL;
        }
        lw_state_machine.state = LW_STATE_IDLE;
    }
    else if ((lw_state_machine.state == LW_STATE_INIT) && (lw_state_machine.data.init_step < 8))
    {
        log_out("HACK 5");
        if (lw_msg_is_ok(message))
        {
            log_out("HACK 5.1");
            lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
            lw_spin_us(LW_MESSAGE_DELAY);
        }
        else
        {
            log_out("Setting not OKed. Retrying.");
            lw_set_config(init_msgs[lw_state_machine.data.init_step]);
            lw_spin_us(LW_MESSAGE_DELAY);
        }
    }
    else if ((lw_state_machine.state == LW_STATE_INIT) && (lw_state_machine.data.init_step == 8)) /*Restart*/
    {
        log_out("HACK 6");
        if (strstr(message, "UART1") == message ||
            strstr(message, "Current work") == message)
        {
            log_debug(DEBUG_LW, "Valid Init 7 response line");
        }
        else if (strcmp(message, "Initialization OK") == 0)
        {
            lw_set_config(init_msgs[lw_state_machine.data.init_step++]);
            log_out("HACK 6.2");
        }
        else
        {
             /*HANDLE ERROR!*/
        }
    }
    else if ((lw_state_machine.state == LW_STATE_INIT) && (lw_state_machine.data.init_step == 9)) /*Join*/
    {
        log_out("HACK 7");
        if (strcmp(message, "OK Join Success") == 0)
        {
            log_out("HACK 7.1");
            lw_state_machine.state = LW_STATE_IDLE;
        }
        /*else error*/
    }
    else if ((lw_state_machine.state != LW_STATE_WAITING_LW_ACK) && (lw_msg_is_unsoclitied(message)))
    {
        /*Done?*/
        log_out("LORA >> (UNSOL) %s", message);
        // lw_command_handle(message);
        return;
    }
    else
    {
        log_error("Unexpected data for LW state.");
    }
    log_out("HACK 8");
    log_out("State: %u", lw_state_machine.state);
    log_out("Init Step: %u", lw_state_machine.data.init_step);
}


static bool lw_parse_recv(char* message, lw_header_t* header)
{
    // at+recv=PORT,RSSI,SNR,DATALEN:DATA
    char recv_msg[] = "at+recv=";
    char* pos = NULL;
    char* next_pos = NULL;

    if (strncmp(message, recv_msg, strlen(recv_msg)) != 0)
    {
        return false;
    }

    pos = message + strlen(recv_msg);
    for (int i = 0; i < 3; i++)
    {
        header->raw[i] = strtol(pos, &next_pos, 10);
        if ((*next_pos) != ',')
        {
            return false;
        }
        pos = next_pos + 1;
    }
    header->datalen = strtol(pos, &next_pos, 10);
    if (*next_pos == '\0')
    {
        header->data = NULL;
        return (header->datalen == 0);
    }
    if (*next_pos == ':')
    {
        header->data = next_pos + 1;
        log_out("string length = %u", strlen(header->data)/2);
        log_out("datalen = %u", (size_t)header->datalen);
        log_out("data = %s", header->data);
        return (strlen(header->data)/2 == (size_t)header->datalen);
    }
    return false;
}


static bool lw_msg_is_unsoclitied(char* message)
{
    lw_header_t header;
    if (!lw_parse_recv(message, &header))
    {
        return false;
    }
    if (header.datalen == 0)
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
    lw_header_t header;
    if (!lw_parse_recv(message, &header))
    {
        // Message does not fit the format
        log_out("Fail 1");
        return false;
    }
    if (header.datalen != 0)
    {
        // Data length not zero so not ack.
        log_out("Fail 2");
        return false;
    }
    log_out("Success");
    return true;
}

enum
{
    LW__ERROR__NOT_CONNECTED =  86, // reconnect
    LW__ERROR__PACKET_SIZE   =  87, // throw away message
    LW__ERROR__TIMEOUT_RX1   =  95, // resend
    LW__ERROR__TIMEOUT_RX2   =  96, // resend
    LW__ERROR__RECV_RX1      =  97, // to be decided
    LW__ERROR__RECV_RX2      =  98, // to be decided
    LW__ERROR__JOIN_FAIL     =  99, // reconnect
    LW__ERROR__DUP_DOWNLINK  = 100, // probably nothing
    LW__ERROR__PAYLOAD_SIZE  = 101, // throw away message
}


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
        case LW__ERROR__NOT_CONNECTED:
            lw_reconnect();
            log_out("Not connected to a network.");
            break;
        case LW__ERROR__JOIN_FAIL:
            lw_reconnect();
            log_out("Failed to join network.");
            break;
        case LW__ERROR__TIMEOUT_RX1:
            // resend
            log_out("RX1 Window timed out.");
            break;
        case LW__ERROR__TIMEOUT_RX2:
            // resend
            log_out("RX1 Window timed out.");
            break;
        default:
            log_out("Error: %"PRIu8, err_no);
            break;
    }
}


void lw_main(void)
{
    lw_packet_t packet;
    packet.confirm = true;
    lw_measurement_t measurements[] =
    {
        { 1, 3, LW_ID__TEMPERATURE, LW_FMT__TEMPERATURE , 272},
        { 2, 5, LW_ID__TEMPERATURE, LW_FMT__TEMPERATURE , 255},
        { (long int)NULL },
    };
    *packet.measurements = *measurements;
    lw_send_packet(&packet);
}
