#include <inttypes.h>
#include <stddef.h>
#include <ctype.h>

#include <libopencm3/cm3/systick.h>

#include "log.h"
#include "pinmap.h"
#include "modbus.h"
#include "uart_rings.h"
#include "uarts.h"
#include "sys_time.h"
#include "cmd.h"
#include "persist_config.h"

#define MODBUS_RESP_TIMEOUT_MS 2000
#define MODBUS_SENT_TIMEOUT_MS 2000

#define MODBUS_ERROR_MASK 0x80

#define MODBUS_BIN_START '{'
#define MODBUS_BIN_STOP '}'


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

static uint32_t modbus_sent_timing_init = 0;
static uint32_t modbus_read_timing_init = 0;

static uint32_t modbus_send_start_delay = 0;
static uint32_t modbus_send_stop_delay = 0;

static bool do_binary_framing = false; /* This is to support pymodbus.framer.binary_framer and http://jamod.sourceforge.net/. */



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
    modbus_debug("Message timeout, dumping left overs.");
    modbuspacket_len = 0;
    modbus_read_timing_init = 0;
    char c;
    while(ring_buf_get_pending(ring))
        ring_buf_read(ring, &c, 1);
    return true;
}


static uint32_t _modbus_get_deci_char_time(unsigned deci_char, unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop)
{
    databits *= 10; /* *10 to support half bit. */
    databits += 10; /* One start bit */

    switch(stop)
    {
        case uart_stop_bits_1   : databits += 10; break;
        case uart_stop_bits_1_5 : databits += 5;  break;
        case uart_stop_bits_2   : databits += 20; break;
        default:
            modbus_debug("Stop bits unknown...assuming 1.");
            databits += 10;
            break;
    }

    if (parity != uart_parity_none)
        databits += 10;

    unsigned bits = deci_char * databits;
    uint32_t r = 1 + 1000 * bits / speed / 100;
    return r;
}


static void _modbus_setup_delays(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop)
{
    if (do_binary_framing)
    {
        modbus_send_start_delay = 0;
        modbus_send_stop_delay = _modbus_get_deci_char_time(10 /*1.0*/, speed, databits, parity, stop);
    }
    else
    {
        modbus_send_start_delay = _modbus_get_deci_char_time(35 /*3.5*/, speed, databits, parity, stop);
        modbus_send_stop_delay = _modbus_get_deci_char_time(35 /*3.5*/, speed, databits, parity, stop);
    }
}


void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing)
{
    uart_resetup(RS485_UART, speed, databits, parity, stop);

    do_binary_framing = binary_framing;

    _modbus_setup_delays(speed, databits, parity, stop);
}



bool modbus_setup_from_str(char * str)
{
    char * pos = skip_space(str);

    bool binary_framing = false;

    if (toupper(pos[0]) == 'R' &&
        toupper(pos[1]) == 'T' &&
        toupper(pos[2]) == 'U')
    {
        binary_framing = false;
    }
    else if (toupper(pos[0]) == 'B' &&
             toupper(pos[1]) == 'I' &&
             toupper(pos[2]) == 'N')
    {
        binary_framing = true;
    }
    else
    {
        log_error("Unknown modbus protocol.");
        return false;
    }

    pos+=3;

    if (!uart_resetup_str(RS485_UART, skip_space(pos)))
        return false;

    unsigned speed;
    uint8_t databits;
    uart_parity_t parity;
    uart_stop_bits_t stop;

    uart_get_setup(RS485_UART, &speed, &databits, &parity, &stop);

    do_binary_framing = binary_framing;

    _modbus_setup_delays(speed, databits, parity, stop);

    modbus_debug("Modbus @ %s %u %u%c%s", (binary_framing)?"BIN":"RTU", speed, databits, uart_parity_as_char(parity), uart_stop_bits_as_str(stop));

    return true;
}




uint32_t modbus_start_delay(void)
{
    return modbus_send_start_delay;
}


uint32_t modbus_stop_delay(void)
{
    return modbus_send_stop_delay;
}


void modbus_use_do_binary_framing(bool enable)
{
    do_binary_framing = enable;
}


bool modbus_start_read(modbus_reg_t * reg)
{
    if (modbus_sent_timing_init)
    {
        uint32_t delta = since_boot_delta(since_boot_ms, modbus_sent_timing_init);
        if (delta > MODBUS_SENT_TIMEOUT_MS)
        {
            modbus_debug("Previous modbus response took timeout.");
            modbus_sent_timing_init = 0;
            current_reg = NULL;
        }
    }

    if (!reg || current_reg || (reg->func != MODBUS_READ_HOLDING_FUNC))
        return false;

    modbus_debug("Reading %"PRIu8" of 0x%"PRIx8":0x%"PRIx8 , reg->reg_count, reg->dev->slave_id, reg->reg_addr);

    /* ADU Header (Application Data Unit) */
    modbuspacket[0] = reg->dev->slave_id;
    /* ====================================== */
    /* PDU payload (Protocol Data Unit) */
    modbuspacket[1] = MODBUS_READ_HOLDING_FUNC; /*Holding*/
    modbuspacket[2] = reg->reg_addr >> 8;   /*Register read address */
    modbuspacket[3] = reg->reg_addr & 0xFF;
    modbuspacket[4] = reg->reg_count >> 8; /*Register read count */
    modbuspacket[5] = reg->reg_count & 0xFF;
    /* ====================================== */
    /* ADU Tail */
    uint16_t crc = modbus_crc(modbuspacket, 6);
    modbuspacket[6] = crc & 0xFF;
    modbuspacket[7] = crc >> 8;

    if (do_binary_framing)
    {
        uart_ring_out(RS485_UART, (char[]){MODBUS_BIN_START}, 1);
        modbuspacket[8] = MODBUS_BIN_STOP;
        uart_ring_out(RS485_UART, (char*)modbuspacket, 9);
    }
    else uart_ring_out(RS485_UART, (char*)modbuspacket, 8); /* Frame is done with silence */

    current_reg = reg;
    modbus_sent_timing_init = since_boot_ms;
    return true;
}


void modbus_ring_process(ring_buf_t * ring)
{
    unsigned len = ring_buf_get_pending(ring);

    if (!modbus_sent_timing_init)
    {
        modbus_debug("Data not expected.");
        char c;
        while(ring_buf_get_pending(ring))
            ring_buf_read(ring, &c, 1);
        return;
    }

    if (!modbuspacket_len)
    {
        if ((!do_binary_framing && len > 2) || (do_binary_framing && len > 3))
        {
            modbus_read_timing_init = 0;
            ring_buf_read(ring, (char*)modbuspacket, 1);

            if (!modbuspacket[0])
            {
                modbus_debug("Zero start");
                return;
            }

            if (do_binary_framing)
            {
                if (modbuspacket[0] != MODBUS_BIN_START)
                {
                    modbus_debug("Not binary frame start.");
                    return;
                }
                ring_buf_read(ring, (char*)modbuspacket, 1);
            }

            ring_buf_read(ring, (char*)modbuspacket + 1, 2);
            uint8_t func = modbuspacket[1];

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
                modbus_debug("Bad modbus function.");
                return;
            }

            if (do_binary_framing)
                modbuspacket_len++;

            modbus_read_timing_init = since_boot_ms;
            modbus_debug("Modbus header received, timer started at:%"PRIu32, modbus_read_timing_init);
        }
        else
        {
            if (!modbus_read_timing_init)
            {
                // There is some bytes, but not enough to be a header yet
                modbus_read_timing_init = since_boot_ms;
                modbus_debug("Modbus bus received, timer started at:%"PRIu32, modbus_read_timing_init);
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
    modbus_debug("Message bytes (%u) reached.", modbuspacket_len);

    ring_buf_read(ring, (char*)modbuspacket + 3, modbuspacket_len);

    if (do_binary_framing)
    {
        if (modbuspacket[modbuspacket_len + 3 - 1] != MODBUS_BIN_STOP)
        {
            modbus_debug("Not binary frame stopped, discarded.");
            return;
        }
        modbuspacket_len--;
    }

    // Now include the header too.
    modbuspacket_len += 3;

    uint16_t crc = modbus_crc(modbuspacket, modbuspacket_len);

    if ( (modbuspacket[modbuspacket_len-1] == (crc >> 8)) &&
         (modbuspacket[modbuspacket_len-2] == (crc & 0xFF)) )
    {
        modbus_debug("Bad CRC");
        modbuspacket_len = 0;
        return;
    }

    modbus_debug("Good CRC");
    modbuspacket_len = 0;

    if (modbuspacket[1] == (MODBUS_READ_HOLDING_FUNC | MODBUS_ERROR_MASK))
    {
        modbus_debug("Exception: %0x"PRIx8, modbuspacket[2]);
        return;
    }

    if (current_reg && (current_reg->dev->slave_id == modbuspacket[0]))
    {
        modbus_reg_t * reg = current_reg;
        current_reg = NULL;
        reg->cb(reg, modbuspacket + 3, modbuspacket[2]);
    }
    else modbus_debug("Unexpected packet");
}


void modbus_init(void)
{
    modbus_bus_config_t config;

    if (persist_get_modbus_bus_config(&config))
    {
        _modbus_setup_delays(config.baudrate, config.databits, config.parity, config.stopbits);
        do_binary_framing = config.binary_protocol;
    }
    else _modbus_setup_delays(MODBUS_SPEED, MODBUS_DATABITS, MODBUS_PARITY, MODBUS_STOP);
}


void modbus_reg_bin_check_cb(modbus_reg_bin_check_t * reg, uint8_t * data, uint8_t size)
{
    if (size != reg->ref_count)
    {
        modbus_debug("Not enough bytes for check. Device disabled.");
        reg->base.dev->enabled = false;
        return;
    }

    for(unsigned n = 0; n < size; n++)
    {
        if (reg->ref[n] != data[n])
        {
            modbus_debug("Bin check failed byte %u. Device disabled.", n);
            reg->base.dev->enabled = false;
            return;
        }
    }

    modbus_debug("Bin check for device %"PRIu8" passed", reg->base.dev->slave_id);
    reg->base.dev->enabled = true;
}


void modbus_reg_ids_check_cb(modbus_reg_ids_check_t * reg, uint8_t * data, uint8_t size)
{
    uint16_t id = data[0] << 8 | data[1];

    modbus_debug("ID check for device %"PRIu8" ID:%"PRIu16, reg->base.dev->slave_id, id);

    for(unsigned n = 0; n < reg->ids_count; n++)
    {
        if (reg->ids[n] == id)
        {
            modbus_debug("ID check passed");
            reg->base.dev->enabled = true;
        }
    }

    modbus_debug("ID check failed");
    reg->base.dev->enabled = false;
}
