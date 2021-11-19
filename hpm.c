#include <stdbool.h>
#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "log.h"
#include "hpm.h"
#include "pinmap.h"
#include "uart_rings.h"

#define hpm_debug(...) log_debug(DEBUG_HPM, "HPM: " __VA_ARGS__)
#define hpm_error(...) log_debug(DEBUG_HPM, "HPM: ERROR: " __VA_ARGS__)


typedef union
{
    uint16_t d;
    struct
    {
        uint8_t l;
        uint8_t h;
    };
} unit_entry_t;

unit_entry_t pm25_entry = {0};
unit_entry_t pm10_entry = {0};

typedef struct
{
    uint8_t id;
    uint8_t len;
} __attribute__((packed)) hmp_packet_header_t;

typedef struct
{
    hmp_packet_header_t header;
    int (*cb)(uint8_t *data);
} hpm_response_t;


static int process_part_measure_response(uint8_t *data);
static int process_part_measure_long_response(uint8_t *data);
static int process_nack_response(uint8_t *data);
static int process_ack_response(uint8_t *data);


static hpm_response_t responses[] =
{
    {{0x40, 8},  process_part_measure_response},
    {{0x42, 32}, process_part_measure_long_response},
    {{0x96, 2},  process_nack_response},
    {{0xA5, 2},  process_ack_response},
    {{0,0}, 0}
};


_Static_assert(CMD_LINELEN >= 32, "Buffer used too small for longest packet");



static int process_part_measure_response(uint8_t *data)
{
    if (data[1] != 5 || data[2] != 0x04)
    {
        hpm_error("Malformed particle measure result.");
        return -1;
    }

    uint16_t cs = 0;

    for(unsigned n = 0; n < 7; n++)
        cs += data[n];

    cs = (65536 - cs) % 256;

    uint8_t checksum = data[7];

    if(checksum != cs)
    {
        hpm_error("hecksum error; got 0x%02x but expected 0x%02x.", checksum, cs);
        return -1;
    }

    pm25_entry.h = data[3];
    pm25_entry.l = data[4];
    pm10_entry.h = data[5];
    pm10_entry.l = data[6];

    return 8;
}


static int process_part_measure_long_response(uint8_t *data)
{

    if (data[1] != 0x4d || data[2] != 0 || data[3] != 28)
    { /* 13 2byte data entries + 2 for byte checksum*/
        hpm_error("Malformed long particle measure result.");
        return -1;
    }

    uint16_t cs = 0;

    for(unsigned n = 0; n < 30; n++)
        cs += data[n];

    unit_entry_t checksum = {.h = data[30], .l = data[31]};

    if (checksum.d != cs)
    {
        hpm_error("checksum error; got 0x%02x but expected 0x%02x.", checksum.d, cs);
        return -1;
    }

    pm25_entry.h = data[6];
    pm25_entry.l = data[7];
    pm10_entry.h = data[8];
    pm10_entry.l = data[9];

    hpm_debug("PM10:%u, PM2.5:%u", (unsigned)pm10_entry.d, (unsigned)pm25_entry.d);
    return 32;
}


static int process_nack_response(uint8_t *data)
{
    if(data[1] == 0x96)
    {
        hpm_error("Negative ACK");
        return 2;
    }
    else hpm_debug("NACK");

    return -1;
}


static int process_ack_response(uint8_t *data)
{
    if (data[1] == 0xA5)
    {
        hpm_debug("ACK received");
        return 2;
    }
    else hpm_error("ACK broken.");

    return -1;
}


void hdm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len)
{
    static hmp_packet_header_t header;
    static bool header_active = false;

    unsigned len = ring_buf_get_pending(ring);

    if (!header_active)
    {
        if (len > sizeof(header))
        {
            char temp;
            ring_buf_read(ring, &temp, 1);
            if (temp)
            {
                header.id = temp;
                char temp;
                ring_buf_read(ring, &temp, 1);
                header.id = len;
                header_active = true;
            }
            else hpm_error("Zero pad");
        }
    }

    if (header_active)
    {
        if (len >= header.len)
        {
            if (header.len <= tmpbuf_len)
            {
                ring_buf_read(ring, tmpbuf, header.len);

                for (hpm_response_t * respond = responses; respond->header.id; respond++)
                {
                    if (header.id == respond->header.id)
                    {
                        hpm_debug("Found 0x%02"PRIx8, header.id);
                        respond->cb((uint8_t*)tmpbuf);
                        header_active = false;
                        break;
                    }
                }

                if (header_active)
                {
                    hpm_error("Packet type 0x%02"PRIx8" len %"PRIu8" unknown.", header.id, header.len);
                    header_active = false;
                }
            }
            else
            {
                hpm_error("Packet type 0x%02"PRIx8" too long, dropping.", header.id);
                while(header.len)
                    header.len -= ring_buf_read(ring, tmpbuf, (header.len > tmpbuf_len)?tmpbuf_len:header.len);
                header_active = false;
            }
        }
    }
}


void hmp_request(void)
{
    // Send the request for the measurement, though it does send measurements on power up, and we power it up/down each time.
    uart_ring_out(HPM_UART, (char*)(uint8_t[]){0x68, 0x01, 0x04, 0x93}, 4);
}


void hmp_enable(bool enable)
{
    port_n_pins_t port_n_pin = HPM_EN_PIN;

    rcc_periph_clock_enable(PORT_TO_RCC(port_n_pin.port));

    gpio_mode_setup(port_n_pin.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, port_n_pin.pins);

    if (enable)
        gpio_set(port_n_pin.port, port_n_pin.pins);
    else
        gpio_clear(port_n_pin.port, port_n_pin.pins);
}


void hpm_init(void)
{
    
    
}
