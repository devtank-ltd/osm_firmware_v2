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
    static const uint16_t crctable[] =
    {
        0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
        0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
        0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
        0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
        0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
        0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
        0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
        0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
        0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
        0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
        0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
        0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
        0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
        0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
        0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
        0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
        0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
        0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
        0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
        0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
        0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
        0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
        0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
        0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
        0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
        0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
        0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
        0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
        0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
        0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
        0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
        0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
    };

    uint16_t crc = 0xffff;
    while(length--)
    {
        uint8_t nTemp = *buf++ ^ crc;
        crc >>= 8;
        crc ^= crctable[nTemp];
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
