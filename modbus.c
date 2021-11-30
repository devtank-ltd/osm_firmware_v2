#include <inttypes.h>
#include <stddef.h>

#include <libopencm3/cm3/systick.h>

#include "log.h"
#include "pinmap.h"
#include "modbus.h"
#include "uart_rings.h"
#include "sys_time.h"

#define MODBUS_RESP_TIMEOUT_MS 2000
#define MODBUS_SENT_TIMEOUT_MS 2000

#define MODBUS_ERROR_MASK 0x80


/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                     <             PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)

    Also good source : http://www.simplymodbus.ca
*/

#define MAX_MODBUS_PACKET_SIZE    127


static uint8_t modbuspacket[MAX_MODBUS_PACKET_SIZE];

static unsigned modbuspacket_len = 0;

static modbus_reg_t * current_reg = NULL;
static modbus_reg_cb * current_reg_cb = NULL;

static uint32_t modbus_sent_timing_init = 0;
static uint32_t modbus_read_timing_init = 0;


uint16_t modbus_crc(uint8_t * buf, unsigned length)
{
    uint16_t crc = 0xFFFF;

    for (unsigned pos = 0; pos < length; pos++)
    {
        crc ^= (uint16_t)buf[pos];        // XOR byte into least sig. byte of crc

        for (int i = 8; i != 0; i--)      // Loop over each bit
        {
            if ((crc & 0x0001) != 0)      // If the LSB is set
            {
                crc >>= 1;                // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else                          // Else LSB is not set
                crc >>= 1;                // Just shift right
        }
    }
    return crc;
}


static bool _modbus_has_timedout(ring_buf_t * ring)
{
    uint32_t delta = since_boot_delta(since_boot_ms, modbus_read_timing_init);
    if (delta < MODBUS_RESP_TIMEOUT_MS)
        return false;
    log_debug(DEBUG_MODBUS, "Modbus message timeout, dumping left overs.");
    modbuspacket_len = 0;
    modbus_read_timing_init = 0;
    char c;
    while(ring_buf_get_pending(ring))
        ring_buf_read(ring, &c, 1);
    return true;
}


bool modbus_start_read(modbus_reg_t * reg, modbus_reg_cb cb)
{
    if (modbus_sent_timing_init)
    {
        uint32_t delta = since_boot_delta(since_boot_ms, modbus_sent_timing_init);
        if (delta > MODBUS_SENT_TIMEOUT_MS)
        {
            log_debug(DEBUG_MODBUS, "Previous modbus response took timeout.");
            modbus_sent_timing_init = 0;
            current_reg = NULL;
            current_reg_cb = NULL;
        }
    }

    if (!reg || !cb || current_reg || (reg->func != MODBUS_READ_HOLDING_FUNC))
        return false;

    log_debug(DEBUG_MODBUS, "Modbus reading %"PRIu8" of 0x%"PRIx8":0x%"PRIx8 , reg->len, reg->slave_id, reg->addr);

    /* ADU Header (Application Data Unit) */
    modbuspacket[0] = reg->slave_id;
    /* ====================================== */
    /* PDU payload (Protocol Data Unit) */
    modbuspacket[1] = MODBUS_READ_HOLDING_FUNC; /*Holding*/
    modbuspacket[2] = reg->addr >> 8;   /*Register read address */
    modbuspacket[3] = reg->addr & 0xFF;
    modbuspacket[4] = reg->len >> 8; /*Register read count */
    modbuspacket[5] = reg->len & 0xFF;
    /* ====================================== */
    /* ADU Tail */
    uint16_t crc = modbus_crc(modbuspacket, 6);
    modbuspacket[6] = crc & 0xFF;
    modbuspacket[7] = crc >> 8;

    unsigned r = uart_ring_out(RS485_UART, (char*)modbuspacket, 8);
    if (r < 8)
    {
        log_debug(DEBUG_MODBUS, "Failed to send all of modbus message.");
        return false;
    }

    current_reg = reg;
    current_reg_cb = cb;
    modbus_sent_timing_init = since_boot_ms;
    return true;
}


void modbus_ring_process(ring_buf_t * ring)
{
    unsigned len = ring_buf_get_pending(ring);

    if (!modbus_sent_timing_init)
    {
        log_debug(DEBUG_MODBUS, "Modbus data not expected.");
        char c;
        while(ring_buf_get_pending(ring))
            ring_buf_read(ring, &c, 1);
        return;
    }

    if (!modbuspacket_len)
    {
        if (len > 2)
        {
            modbus_read_timing_init = 0;
            ring_buf_read(ring, (char*)modbuspacket, 1);

            if (!modbuspacket[0])
            {
                log_debug(DEBUG_MODBUS, "Zero start");
                return;
            }
            if (!current_reg || (current_reg->slave_id != modbuspacket[0]))
            {
                log_debug(DEBUG_MODBUS, "Possible slave ID 0x%"PRIx8" doesn't match slave ID waiting for.", modbuspacket[0]);
                return;
            }
            ring_buf_read(ring, (char*)modbuspacket + 1, 2);
            uint8_t func = modbuspacket[1];
            if ((func & ~MODBUS_ERROR_MASK) != current_reg->func)
            {
                log_debug(DEBUG_MODBUS, "Possible func ID doesn't match func ID waiting for.");
                return;
            }
            len -= 3;
            if (func == MODBUS_READ_HOLDING_FUNC)
            {
                modbuspacket_len = modbuspacket[2] + 2 /* result data and crc*/;
            }
            else if (func == (current_reg->func | MODBUS_ERROR_MASK))
            {
                modbuspacket_len = 3; /* Exception code and crc*/
            }
            else
            {
                log_debug(DEBUG_MODBUS, "Bad modbus function.");
                return;
            }
            modbus_read_timing_init = since_boot_ms;
            log_debug(DEBUG_MODBUS, "Modbus header received, timer started at:%"PRIu32, modbus_read_timing_init);
        }
        else
        {
            if (!modbus_read_timing_init)
            {
                // There is some bytes, but not enough to be a header yet
                modbus_read_timing_init = since_boot_ms;
                log_debug(DEBUG_MODBUS, "Modbus bus received, timer started at:%"PRIu32, modbus_read_timing_init);
            }
            else _modbus_has_timedout(ring);
            return;
        }
    }

    if (!modbuspacket_len)
        return;

    if (len < modbuspacket_len)
    {
        _modbus_has_timedout(ring);
        return;
    }

    modbus_read_timing_init = 0;
    log_debug(DEBUG_MODBUS, "Modbus message bytes (%u) reached.", modbuspacket_len);

    ring_buf_read(ring, (char*)modbuspacket + 3, modbuspacket_len);

    // Now include the header too.
    modbuspacket_len += 3;

    uint16_t crc = modbus_crc(modbuspacket, modbuspacket_len);

    if ( (modbuspacket[modbuspacket_len-1] == (crc >> 8)) &&
         (modbuspacket[modbuspacket_len-2] == (crc & 0xFF)) )
    {
        log_debug(DEBUG_MODBUS, "Bad modbus CRC");
        modbuspacket_len = 0;
        return;
    }

    log_debug(DEBUG_MODBUS, "Good modbus CRC");

    if (modbuspacket[1] == (MODBUS_READ_HOLDING_FUNC | MODBUS_ERROR_MASK))
    {
        log_debug(DEBUG_MODBUS, "Modbus Exception: %0x"PRIx8, modbuspacket[2]);
        return;
    }

    if (current_reg_cb)
        current_reg_cb(current_reg, modbuspacket + 3, modbuspacket[2]);

    current_reg = NULL;
    current_reg_cb = NULL;
    modbuspacket_len = 0;
}


void modbus_init(void)
{
}
