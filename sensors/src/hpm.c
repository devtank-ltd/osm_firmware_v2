#include <stdbool.h>
#include <inttypes.h>

#include "log.h"
#include "hpm.h"
#include "uart_rings.h"
#include "common.h"
#include "platform.h"
#include "uarts.h"


#define MEASUREMENTS_COLLECT_TIME_HPM_MS         10000
#define HPM_TIMEOUT_MS                           (uint32_t)(MEASUREMENTS_COLLECT_TIME_HPM_MS * 1.5)

#define hpm_error(...) hpm_debug("ERROR: " __VA_ARGS__)


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

static bool     hpm_valid               = false;
static bool     hpm_is_on               = false;
static uint32_t _hpm_start_time         = 0;
static uint32_t _hpm_last_collect_time  = MEASUREMENTS_COLLECT_TIME_HPM_MS;

typedef struct
{
    uint8_t id;
    uint8_t len;
} __attribute__((packed)) hpm_packet_header_t;

typedef struct
{
    hpm_packet_header_t header;
    void (*cb)(const uint8_t *data);
} hpm_response_t;


void hpm_enable(bool enable);
static void process_part_measure_response(const uint8_t *data);
static void process_part_measure_long_response(const uint8_t *data);
static void process_nack_response(const uint8_t *data);
static void process_ack_response(const uint8_t *data);


static hpm_response_t responses[] =
{
    {{0x40, 8},  process_part_measure_response},
    {{0x42, 32}, process_part_measure_long_response},
    {{0x96, 2},  process_nack_response},
    {{0xA5, 2},  process_ack_response},
    {{0,0}, 0}
};


_Static_assert(CMD_LINELEN >= 32, "Buffer used too small for longest packet");


static void _hpm_is_valid(void)
{
    hpm_valid = true;
    _hpm_last_collect_time = since_boot_delta(get_since_boot_ms(), _hpm_start_time);
}


static void process_part_measure_response(const uint8_t *data)
{
    if (data[1] != 5 || data[2] != 0x04)
    {
        hpm_error("Malformed particle measure result.");
        return;
    }

    uint16_t cs = 0;

    for(unsigned n = 0; n < 7; n++)
        cs += data[n];

    cs = (65536 - cs) % 256;

    uint8_t checksum = data[7];

    if(checksum != cs)
    {
        hpm_error("hecksum error; got 0x%02x but expected 0x%02x.", checksum, cs);
        return;
    }

    pm25_entry.h = data[3];
    pm25_entry.l = data[4];
    pm10_entry.h = data[5];
    pm10_entry.l = data[6];
    _hpm_is_valid();
}


static void process_part_measure_long_response(const uint8_t *data)
{
    if (data[1] != 0x4d || data[2] != 0 || data[3] != 28)
    { /* 13 2byte data entries + 2 for byte checksum*/
        hpm_error("Malformed long particle measure result.");
        return;
    }

    uint16_t cs = 0;

    for(unsigned n = 0; n < 30; n++)
        cs += data[n];

    unit_entry_t checksum = {.h = data[30], .l = data[31]};

    if (checksum.d != cs)
    {
        hpm_error("checksum error; got 0x%02x but expected 0x%02x.", checksum.d, cs);
        return;
    }

    pm25_entry.h = data[6];
    pm25_entry.l = data[7];
    pm10_entry.h = data[8];
    pm10_entry.l = data[9];

    static unsigned message_count = 0;

    /* First seems to always be 0, second always seems to be high,
     * third seems alright, fourth is better. */
    if (message_count > 2)
    {
        hpm_valid = true;
        if (hpm_is_on)
        {
            uart_enable(HPM_UART, false);
            hpm_enable(false);
            uart_rings_in_wipe(HPM_UART);
            uart_rings_out_wipe(HPM_UART);
        }
        message_count = 0;
    }
    else
    {
        message_count++;
    }

    hpm_debug("PM10:%u, PM2.5:%u", (unsigned)pm10_entry.d, (unsigned)pm25_entry.d);
}


static void process_nack_response(const uint8_t *data)
{
    if(data[1] == 0x96)
        hpm_error("Negative ACK");
    else
        hpm_debug("NACK");
}


static void process_ack_response(const uint8_t *data)
{
    if (data[1] == 0xA5)
        hpm_debug("ACK received");
    else
        hpm_error("ACK broken.");
}


void hpm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len)
{
    static hpm_packet_header_t header;
    static bool header_active = false;

    unsigned len = ring_buf_get_pending(ring);

    if (!header_active)
    {
        uint8_t id;
        ring_buf_read(ring, (char*)&id, 1);
        len-=1;

        for (hpm_response_t * respond = responses; respond->header.id; respond++)
        {
            if (id == respond->header.id)
            {
                header = respond->header;
                header.len -= 1; /*Change to remaining to read, not total length.*/
                header_active = true;
                break;
            }
        }
    }

    if (!header_active)
        return;

    if (len >= header.len)
    {
        if (header.len <= tmpbuf_len)
        {
            tmpbuf[0] = header.id;
            ring_buf_read(ring, (tmpbuf + 1), header.len);

            for (hpm_response_t * respond = responses; respond->header.id; respond++)
            {
                if (header.id == respond->header.id)
                {
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

/*
void hpm_request(void)
{
    // Send the request for the measurement, though it does send measurements on power up, and we power it up/down each time.
    uart_ring_out(HPM_UART, (char*)(uint8_t[]){0x68, 0x01, 0x04, 0x93}, 4);
}*/


void hpm_enable(bool enable)
{
    static unsigned hpm_use_ref = 0;

    platform_hpm_enable(enable);

    hpm_is_on = enable;
    if (enable)
    {
        _hpm_start_time = get_since_boot_ms();
        hpm_use_ref++;
        hpm_debug("Power On (ref:%u)", hpm_use_ref);
        return;
    }

    if (hpm_use_ref)
        hpm_use_ref--;
    if (!hpm_use_ref)
    {
        hpm_debug("Power Off");
        hpm_valid = false;
    }
    else
        hpm_debug("Power Off deferred (ref:%u)", hpm_use_ref);
}



static measurements_sensor_state_t _hpm_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = _hpm_last_collect_time * 1.1f;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _hpm_get_pm10(char* name, measurements_reading_t* val)
{
    if (!val)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!hpm_valid)
    {
        if (since_boot_delta(get_since_boot_ms(), _hpm_start_time) < HPM_TIMEOUT_MS)
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        hpm_enable(false);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    val->v_i64 = (int64_t)pm10_entry.d;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _hpm_get_pm25(char* name, measurements_reading_t* val)
{
    if (!val)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!hpm_valid)
    {
        if (since_boot_delta(get_since_boot_ms(), _hpm_start_time) < HPM_TIMEOUT_MS)
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        hpm_enable(false);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    val->v_i64 = (int64_t)pm25_entry.d;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _hpm_init(char* name, bool in_isolation)
{
    if (!hpm_is_on)
    {
        uart_enable(HPM_UART, true);
        hpm_enable(true);
    }
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _hpm_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void hpm_pm10_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _hpm_collection_time;
    inf->init_cb            = _hpm_init;
    inf->get_cb             = _hpm_get_pm10;
    inf->value_type_cb      = _hpm_value_type;
}


void hpm_pm25_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _hpm_collection_time;
    inf->init_cb            = _hpm_init;
    inf->get_cb             = _hpm_get_pm25;
    inf->value_type_cb      = _hpm_value_type;
}
