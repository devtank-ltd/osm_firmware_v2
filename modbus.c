#include <inttypes.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

#include <libopencm3/cm3/systick.h>

#include "log.h"
#include "pinmap.h"
#include "modbus.h"
#include "uart_rings.h"
#include "uarts.h"
#include "common.h"
#include "cmd.h"
#include "persist_config.h"

#define MODBUS_RESP_TIMEOUT_MS 2000
#define MODBUS_SENT_TIMEOUT_MS 2000
#define MODBUS_TX_GAP_MS        100

#define MODBUS_ERROR_MASK 0x80

#define MODBUS_BIN_START '{'
#define MODBUS_BIN_STOP '}'

#define MODBUS_SLOTS  (MODBUS_MAX_DEV * MODBUS_DEV_REGS)

typedef void (*modbus_reg_cb)(modbus_reg_t * reg, uint8_t * data, uint8_t size);


/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                     <             PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)

    Also good source : http://www.simplymodbus.ca
*/

#define MAX_MODBUS_PACKET_SIZE    127

static modbus_bus_t * modbus_bus = NULL;
static modbus_dev_t * modbus_devices = NULL;

static uint8_t modbuspacket[MAX_MODBUS_PACKET_SIZE];

static unsigned modbuspacket_len = 0;

static char _message_queue_regs[1 + sizeof(modbus_reg_t*) * MODBUS_SLOTS] = {0};

static ring_buf_t _message_queue = RING_BUF_INIT(_message_queue_regs, sizeof(_message_queue_regs));


static uint32_t modbus_read_timing_init = 0;
static uint32_t modbus_read_last_good = 0;
static uint32_t modbus_cur_send_time = 0;
static bool     modbus_want_rx = false;
static bool     modbus_has_rx = false;

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
    modbus_debug("Modbus @ %s %u %u%c%s", (do_binary_framing)?"BIN":"RTU", speed, databits, uart_parity_as_char(parity), uart_stop_bits_as_str(stop));

    modbus_bus->baudrate    = speed;
    modbus_bus->databits    = databits;
    modbus_bus->parity      = parity;
    modbus_bus->stopbits    = stop;
    modbus_bus->binary_protocol = do_binary_framing;
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


static void _modbus_do_start_read(modbus_reg_t * reg)
{
    modbus_dev_t * dev = modbus_reg_get_dev(reg);

    unsigned reg_count = 1;

    switch (reg->type)
    {
        case MODBUS_REG_TYPE_U16    : reg_count = 1; break;
        case MODBUS_REG_TYPE_U32    : reg_count = 2; break;
        case MODBUS_REG_TYPE_FLOAT  : reg_count = 2; break;
        default: break;
    }

    modbus_debug("Reading %"PRIu8" of %."STR(MODBUS_NAME_LEN)"s (0x%"PRIx8":0x%"PRIx16")" , reg_count, reg->name, dev->slave_id, reg->reg_addr);

    unsigned body_size = 4;

    if (reg->func == MODBUS_READ_HOLDING_FUNC)
    {
        /* ADU Header (Application Data Unit) */
        modbuspacket[0] = dev->slave_id;
        /* ====================================== */
        /* PDU payload (Protocol Data Unit) */
        modbuspacket[1] = MODBUS_READ_HOLDING_FUNC; /*Holding*/
        modbuspacket[2] = reg->reg_addr >> 8;   /*Register read address */
        modbuspacket[3] = reg->reg_addr & 0xFF;
        modbuspacket[4] = reg_count >> 8; /*Register read count */
        modbuspacket[5] = reg_count & 0xFF;
        body_size = 6;
        /* ====================================== */
    }
    else if (reg->func == MODBUS_READ_INPUT_FUNC)
    {
        /* ADU Header (Application Data Unit) */
        modbuspacket[0] = dev->slave_id;
        /* ====================================== */
        /* PDU payload (Protocol Data Unit) */
        modbuspacket[1] = MODBUS_READ_INPUT_FUNC; /*Input*/
        modbuspacket[2] = reg->reg_addr >> 8;   /*Register read address */
        modbuspacket[3] = reg->reg_addr & 0xFF;
        modbuspacket[4] = reg_count >> 8; /*Register read count */
        modbuspacket[5] = reg_count & 0xFF;
        body_size = 6;
        /* ====================================== */
    }
    /* ADU Tail */
    uint16_t crc = modbus_crc(modbuspacket, body_size);
    modbuspacket[body_size] = crc & 0xFF;
    modbuspacket[body_size+1] = crc >> 8;

    modbus_want_rx = true;
    modbus_cur_send_time = get_since_boot_ms();

    if (do_binary_framing)
    {
        uart_ring_out(RS485_UART, (char[]){MODBUS_BIN_START}, 1);
        modbuspacket[8] = MODBUS_BIN_STOP;
        uart_ring_out(RS485_UART, (char*)modbuspacket, 9);
    }
    else uart_ring_out(RS485_UART, (char*)modbuspacket, 8); /* Frame is done with silence */

    /* All current types use this as is_valid. */
    reg->class_data_b = 0;
}


bool modbus_start_read(modbus_reg_t * reg)
{
    modbus_dev_t * dev = modbus_reg_get_dev(reg);

    if (!dev ||
        !(reg->func == MODBUS_READ_HOLDING_FUNC || reg->func == MODBUS_READ_INPUT_FUNC) ||
        !(reg->type == MODBUS_REG_TYPE_U16 || reg->type == MODBUS_REG_TYPE_U32 || reg->type == MODBUS_REG_TYPE_FLOAT))
    {
        log_error("Modbus register \"%."STR(MODBUS_NAME_LEN)"s\" unable to start read.", reg->name);
        return false;
    }

    if (ring_buf_is_full(&_message_queue))
    {
        if (modbus_has_rx)
        {
            uint32_t delta = since_boot_delta(get_since_boot_ms(), modbus_read_last_good);
            if (delta < MODBUS_SENT_TIMEOUT_MS)
            {
                modbus_debug("No slot free.. comms??");
                return false;
            }
        }
        modbus_debug("Previous comms issue. Restarting slots.");
        ring_buf_clear(&_message_queue);

        modbus_read_last_good = 0;
        modbus_cur_send_time = 0;
        modbus_has_rx = false;
        modbus_want_rx = false;
    }

    if (!ring_buf_add_data(&_message_queue, &reg, sizeof(modbus_reg_t*)))
    {
        log_error("Modbus queue error");
        ring_buf_clear(&_message_queue);
        return false;
    }

    if (modbus_want_rx)
    {
        modbus_debug("Deferred read of \"%."STR(MODBUS_NAME_LEN)"s\"", reg->name);
        return true;
    }

    modbus_debug("Immediate read");
    _modbus_do_start_read(reg);
    return true;
}


static void _modbus_next_message(void)
{
    modbus_reg_t * current_reg = NULL;

    if (ring_buf_peek(&_message_queue, (char*)&current_reg, sizeof(current_reg)) != sizeof(current_reg))
        return;

    if (current_reg)
        _modbus_do_start_read(current_reg);
}


static bool _modbus_has_timedout(ring_buf_t * ring)
{
    uint32_t delta = (modbus_read_timing_init)?
                    since_boot_delta(get_since_boot_ms(), modbus_read_timing_init)
                    :
                    since_boot_delta(get_since_boot_ms(), modbus_cur_send_time);

    if (delta < MODBUS_RESP_TIMEOUT_MS)
        return false;
    modbus_debug("Message timeout, dumping left overs.");
    modbuspacket_len = 0;
    modbus_read_timing_init = 0;
    modbus_want_rx = false;
    modbus_has_rx = false;
    ring_buf_clear(ring);
    _modbus_next_message();
    return true;
}

static void _modbus_reg_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size);

void modbus_ring_process(ring_buf_t * ring)
{
    if (!modbus_want_rx)
    {
        ring_buf_clear(ring);

        if (ring_buf_get_pending(&_message_queue))
        {
            uint32_t delta = since_boot_delta(get_since_boot_ms(), modbus_read_last_good);
            if (delta > MODBUS_TX_GAP_MS)
                _modbus_next_message();
        }
        return;
    }

    if (_modbus_has_timedout(ring))
        return;

    unsigned len = ring_buf_get_pending(ring);

    if (!len)
        return;

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
            else if (func == MODBUS_READ_INPUT_FUNC)
            {
                modbuspacket_len = modbuspacket[2] + 2 /* result data and crc*/;
            }
            else if ((func & MODBUS_ERROR_MASK) == MODBUS_ERROR_MASK)
            {
                modbuspacket_len = 3; /* Exception code and crc*/
            }
            else
            {
                modbus_debug("Bad function : %"PRIu8, func);
                return;
            }

            if (do_binary_framing)
                modbuspacket_len++;

            modbus_read_timing_init = get_since_boot_ms();
            modbus_debug("header received, timer started at:%"PRIu32, modbus_read_timing_init);
        }
        else
        {
            if (!modbus_read_timing_init)
            {
                // There is some bytes, but not enough to be a header yet
                modbus_read_timing_init = get_since_boot_ms();
                modbus_debug("bus received, timer started at:%"PRIu32, modbus_read_timing_init);
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
    modbus_read_last_good = get_since_boot_ms();
    modbus_has_rx = true;
    modbus_want_rx = false;

    modbus_reg_t * current_reg = NULL;

    if (ring_buf_read(&_message_queue, (char*)&current_reg, sizeof(current_reg)) != sizeof(current_reg) || current_reg == NULL)
    {
        log_error("Modbus comms issues!");
        return;
    }

    if ((modbuspacket[1] == (MODBUS_READ_HOLDING_FUNC | MODBUS_ERROR_MASK)) ||
        (modbuspacket[1] == (MODBUS_READ_INPUT_FUNC | MODBUS_ERROR_MASK)))
    {
        modbus_debug("Exception: %0x"PRIx8, modbuspacket[2]);
        return;
    }

    modbus_dev_t * dev = modbus_reg_get_dev(current_reg);

    if (dev->slave_id != modbuspacket[0])
    {
        log_error("Modbus comms issues!");
        return;
    }

    _modbus_reg_cb(current_reg, modbuspacket + 3, modbuspacket[2]);
}


static void _modbus_reg_set(modbus_reg_t * reg, uint32_t v)
{
    reg->class_data_a = v;
    reg->class_data_b = true;
}

static void _modbus_reg_u16_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    if (size != 2)
        return;
    uint16_t v = data[0] << 8 | data[1];
    _modbus_reg_set(reg, v);
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s U16:%"PRIu16, reg->name, v);
}

static void _modbus_reg_u32_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    if (size != 4)
        return;
    uint32_t v = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    _modbus_reg_set(reg, v);
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s U32:%"PRIu32, reg->name, v);
}

static void _modbus_reg_float_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    if (size != 4)
        return;
    uint32_t v = data[2] << 24 | data[3] << 16 | data[0] << 8 | data[1];
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s F32:%f", reg->name, *(float*)&v);
    #pragma GCC diagnostic pop
    _modbus_reg_set(reg, v);
}

static void _modbus_reg_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    switch(reg->type)
    {
        case MODBUS_REG_TYPE_U16:    _modbus_reg_u16_cb(reg, data, size); break;
        case MODBUS_REG_TYPE_U32:    _modbus_reg_u32_cb(reg, data, size); break;
        case MODBUS_REG_TYPE_FLOAT:  _modbus_reg_float_cb(reg, data, size); break;
        default: log_error("Unknown modbus reg type."); break;
    }
}


void modbus_init(void)
{
    modbus_bus = persist_get_modbus_bus();

    modbus_devices = modbus_bus->modbus_devices;

    if (modbus_bus->version == MODBUS_BLOB_VERSION &&
        modbus_bus->max_dev_num == MODBUS_MAX_DEV      &&
        modbus_bus->max_reg_num == MODBUS_DEV_REGS)
    {
        modbus_debug("Loaded modbus defs");
    }
    else
    {
        modbus_debug("Failed to load modbus defs");
        memset(modbus_devices, 0, sizeof(modbus_dev_t) * MODBUS_MAX_DEV);
        modbus_bus->version = MODBUS_BLOB_VERSION;
        modbus_bus->max_dev_num = MODBUS_MAX_DEV;
        modbus_bus->max_reg_num = MODBUS_DEV_REGS;
        modbus_bus->baudrate    = MODBUS_SPEED;
        modbus_bus->databits    = MODBUS_DATABITS;
        modbus_bus->parity      = MODBUS_PARITY;
        modbus_bus->stopbits    = MODBUS_STOP;
        modbus_bus->binary_protocol = false;
    }

    modbus_setup(modbus_bus->baudrate,
                 modbus_bus->databits,
                 modbus_bus->parity,
                 modbus_bus->stopbits,
                 modbus_bus->binary_protocol);
}
