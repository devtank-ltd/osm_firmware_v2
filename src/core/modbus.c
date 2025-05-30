#include <inttypes.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <osm/core/log.h>
#include <osm/core/modbus.h>
#include <osm/core/modbus_mem.h>
#include <osm/core/uart_rings.h>
#include <osm/core/uarts.h>
#include <osm/core/common.h>
#include <osm/core/cmd.h>
#include <osm/core/persist_config.h>
#include <osm/core/platform.h>
#include "pinmap.h"

#define MODBUS_TX_GAP_MS        100

#define MODBUS_ERROR_MASK 0x80

#define MODBUS_BIN_START '{'
#define MODBUS_BIN_STOP '}'

#define MODBUS_SLOTS  (MODBUS_BLOCKS - 1)

#define MODBUS_MAX_RETRANSMITS 10

/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                     <             PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)

    Also good source : https://www.simplymodbus.ca
*/

#define MAX_MODBUS_PACKET_SIZE    127
#define MODBUS_PACKET_BUF_SIZ     20


static uint8_t modbuspacket[MAX_MODBUS_PACKET_SIZE];
static uint8_t tx_modbuspacket[MODBUS_PACKET_BUF_SIZ];

static unsigned modbuspacket_len = 0;

static char _message_queue_regs[1 + sizeof(modbus_reg_t*) * MODBUS_SLOTS] = {0};

static ring_buf_t _message_queue = RING_BUF_INIT(_message_queue_regs, sizeof(_message_queue_regs));


static uint32_t modbus_read_timing_init = 0;
static uint32_t modbus_read_last_good = 0;
static uint32_t modbus_cur_send_time = 0;
static bool     modbus_want_rx = false;

static uint32_t modbus_send_start_delay = 0;
static uint32_t modbus_send_stop_delay = 0;

static uint32_t modbus_retransmit_count = 0;

static unsigned _echo_bytes = 0;


static struct
{
    union
    {
        uint8_t  unit_id;
        uint8_t  not_done;  /* If there is a unit id (and it's can't be zero and value) then it is currently setting. */
    };
    union
    {
        uint16_t reg_addr;
        uint16_t passfail; /* Only valid if not_done is false. */
    };
    union
    {
        uint16_t value;         /* For MODBUS_WRITE_SINGLE_HOLDING_FUNC   */
        uint16_t num_written;   /* For MODBUS_WRITE_MULTIPLE_HOLDING_FUNC */
    };
} _modbus_reg_set_expected = {0};


/* By default, assume no echo. */
bool modbus_requires_echo_removal() { return false; }


static uint32_t _modbus_get_deci_char_time(unsigned deci_char, unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop)
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


static void _modbus_setup_delays(unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop)
{
    if (modbus_bus->binary_protocol)
    {
        modbus_send_start_delay = 0;
        modbus_send_stop_delay = _modbus_get_deci_char_time(10 /*1.0*/, speed, databits, parity, stop);
    }
    else
    {
        modbus_send_start_delay = _modbus_get_deci_char_time(35 /*3.5*/, speed, databits, parity, stop);
        modbus_send_stop_delay = _modbus_get_deci_char_time(35 /*3.5*/, speed, databits, parity, stop);
    }
    modbus_debug("Modbus @ %s %u %u%c%s", (modbus_bus->binary_protocol)?"BIN":"RTU", speed, databits, osm_uart_parity_as_char(parity), osm_uart_stop_bits_as_str(stop));

    modbus_bus->baudrate    = speed;
    modbus_bus->databits    = databits;
    modbus_bus->parity      = parity;
    modbus_bus->stopbits    = stop;
}


bool modbus_setup_from_str(char * str, cmd_ctx_t * ctx)
{
    /*<BIN/RTU> <SPEED> <BITS><PARITY><STOP>
     * EXAMPLE: RTU 115200 8N1
     */
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
        cmd_ctx_error(ctx,"Unknown modbus protocol.");
        return false;
    }

    pos+=3;

    if (!uart_resetup_str(EXT_UART, skip_space(pos), ctx))
        return false;

    unsigned speed;
    uint8_t databits;
    osm_uart_parity_t parity;
    osm_uart_stop_bits_t stop;

    uart_get_setup(EXT_UART, &speed, &databits, &parity, &stop);

    modbus_bus->binary_protocol = binary_framing;

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


bool modbus_has_pending(void)
{
    return (modbus_want_rx || ring_buf_get_pending(&_message_queue));
}


bool modbus_get_reg_set_expected_done(bool * passfail)
{
    if (passfail)
    {
        if (!_modbus_reg_set_expected.not_done) /* You can't read passfail if not_done is true. */
            *passfail = _modbus_reg_set_expected.passfail;
        else
            *passfail = false;
    }
    return (_modbus_reg_set_expected.not_done)?false:true;
}


static void _modbus_do_start_read(modbus_reg_t * reg)
{
    uint8_t unit_id = modbus_reg_get_unit_id(reg);

    unsigned reg_count = 1;

    switch (reg->type)
    {
        case MODBUS_REG_TYPE_U16    : reg_count = 1; break;
        case MODBUS_REG_TYPE_I16    : reg_count = 1; break;
        case MODBUS_REG_TYPE_U32    : reg_count = 2; break;
        case MODBUS_REG_TYPE_I32    : reg_count = 2; break;
        case MODBUS_REG_TYPE_FLOAT  : reg_count = 2; break;
        default: break;
    }

    modbus_debug("Reading %"PRIu8" of %."STR(MODBUS_NAME_LEN)"s (0x%"PRIx8":0x%"PRIx16")" , reg_count, reg->name, unit_id, reg->reg_addr);

    unsigned body_size = 4;

    if (reg->func == MODBUS_READ_HOLDING_FUNC)
    {
        /* ADU Header (Application Data Unit) */
        tx_modbuspacket[0] = unit_id;
        /* ====================================== */
        /* PDU payload (Protocol Data Unit) */
        tx_modbuspacket[1] = MODBUS_READ_HOLDING_FUNC; /*Holding*/
        tx_modbuspacket[2] = reg->reg_addr >> 8;   /*Register read address */
        tx_modbuspacket[3] = reg->reg_addr & 0xFF;
        tx_modbuspacket[4] = reg_count >> 8; /*Register read count */
        tx_modbuspacket[5] = reg_count & 0xFF;
        body_size = 6;
        /* ====================================== */
    }
    else if (reg->func == MODBUS_READ_INPUT_FUNC)
    {
        /* ADU Header (Application Data Unit) */
        tx_modbuspacket[0] = unit_id;
        /* ====================================== */
        /* PDU payload (Protocol Data Unit) */
        tx_modbuspacket[1] = MODBUS_READ_INPUT_FUNC; /*Input*/
        tx_modbuspacket[2] = reg->reg_addr >> 8;   /*Register read address */
        tx_modbuspacket[3] = reg->reg_addr & 0xFF;
        tx_modbuspacket[4] = reg_count >> 8; /*Register read count */
        tx_modbuspacket[5] = reg_count & 0xFF;
        body_size = 6;
        /* ====================================== */
    }
    /* ADU Tail */
    uint16_t crc = modbus_crc(tx_modbuspacket, body_size);
    tx_modbuspacket[body_size] = crc & 0xFF;
    tx_modbuspacket[body_size+1] = crc >> 8;

    modbus_want_rx = true;
    modbus_cur_send_time = get_since_boot_ms();

    if (modbus_bus->binary_protocol)
    {
        uart_ring_out(EXT_UART, (char[]){MODBUS_BIN_START}, 1);
        tx_modbuspacket[8] = MODBUS_BIN_STOP;
        uart_ring_out(EXT_UART, (char*)tx_modbuspacket, 9);
        if (modbus_requires_echo_removal())
            _echo_bytes = 10;
    }
    else
    {
        uart_ring_out(EXT_UART, (char*)tx_modbuspacket, 8); /* Frame is done with silence */
        if (modbus_requires_echo_removal())
            _echo_bytes = 8;
    }
    reg->value_state = MB_REG_WAITING;
}


static bool _modbus_append_u16(uint8_t* arr, unsigned len, uint16_t v, modbus_byte_orders_t byte_order)
{
    if (len <= 2)
    {
        modbus_debug("Ran out of space in array for appending u16.");
        return false;
    }
    /* TODO: Figure out which order is which */
    if (byte_order == MODBUS_BYTE_ORDER_MSB)
    {
        arr[0] = v >> 8;
        arr[1] = v & 0xFF;
    }
    else if (byte_order == MODBUS_BYTE_ORDER_LSB)
        memcpy(arr, &v, 2);
    else
    {
        modbus_debug("Unknown byte order.");
        return false;
    }
    return true;
}


/* For STM32 and x86-64 are both little-endian, meaning 0x0A0B0C0D is
 * stored:
 * a     : 0x0D
 * a + 1 : 0x0C
 * a + 2 : 0x0B
 * a + 3 : 0x0A
 * => This is LSB
 */


static bool _modbus_append_u32(uint8_t* arr, unsigned len, uint32_t v, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (len <= 4)
    {
        modbus_debug("Ran out of space in array for appending u32.");
        return false;
    }
    if (word_order == MODBUS_WORD_ORDER_MSW)
    {
        if (!_modbus_append_u16(&arr[0], len, v >> 16, byte_order))
            return false;
        if (!_modbus_append_u16(&arr[2], len - 2, v & 0xFFFF, byte_order))
            return false;
    }
    else if (word_order == MODBUS_WORD_ORDER_LSW)
    {
        if (!_modbus_append_u16(&arr[0], len, v & 0xFFFF, byte_order))
            return false;
        if (!_modbus_append_u16(&arr[2], len - 2, v >> 16, byte_order))
            return false;
    }
    else
    {
        modbus_debug("Unknown word order.");
        return false;
    }
    return true;
}


static bool _modbus_append_value(uint8_t* arr, unsigned len, modbus_reg_type_t type, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order, uint32_t* value, unsigned* body_size)
{
    switch (type)
    {
        case MODBUS_REG_TYPE_U16:
        {
            *body_size = *body_size + 2;
            return _modbus_append_u16(arr, len, *value, byte_order);
        }
        case MODBUS_REG_TYPE_I16:
        {
            *body_size = *body_size + 2;
            int16_t vi16 = *value;
            uint16_t vu16 = *((uint16_t*)&vi16);
            return _modbus_append_u16(arr, len, vu16, byte_order);
        }
        case MODBUS_REG_TYPE_U32:
        {
            *body_size = *body_size + 4;
            return _modbus_append_u32(arr, len, *value, byte_order, word_order);
        }
        case MODBUS_REG_TYPE_FLOAT:
        /* Fall through */
        case MODBUS_REG_TYPE_I32:
        {
            *body_size = *body_size + 4;
            int32_t vi32 = *value;
            uint32_t vu32 = *((uint32_t*)&vi32);
            return _modbus_append_u32(arr, len, vu32, byte_order, word_order);
        }
        default:
            return false;
    }
    return true;
}


bool modbus_set_reg(uint16_t unit_id, uint16_t reg_addr, uint8_t func, modbus_reg_type_t type, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order, float value)
{
    unsigned reg_count = 1;

    switch (type)
    {
        case MODBUS_REG_TYPE_U16    : reg_count = 1; break;
        case MODBUS_REG_TYPE_I16    : reg_count = 1; break;
        case MODBUS_REG_TYPE_U32    : reg_count = 2; break;
        case MODBUS_REG_TYPE_I32    : reg_count = 2; break;
        case MODBUS_REG_TYPE_FLOAT  : reg_count = 2; break;
        default:
            modbus_debug("Unknown type.");
            return false;
    }

    unsigned body_size = 4;

    if (func == MODBUS_WRITE_SINGLE_HOLDING_FUNC)
    {
        tx_modbuspacket[0] = unit_id;
        tx_modbuspacket[1] = MODBUS_WRITE_SINGLE_HOLDING_FUNC;
        tx_modbuspacket[2] = reg_addr >> 8;
        tx_modbuspacket[3] = reg_addr & 0xFF;
        body_size = 4;
    }
    else if (func == MODBUS_WRITE_MULTIPLE_HOLDING_FUNC)
    {
        tx_modbuspacket[0] = unit_id;
        tx_modbuspacket[1] = MODBUS_WRITE_MULTIPLE_HOLDING_FUNC;
        tx_modbuspacket[2] = reg_addr >> 8;
        tx_modbuspacket[3] = reg_addr & 0xFF;
        tx_modbuspacket[4] = reg_count >> 8;
        tx_modbuspacket[5] = reg_count & 0xFF;
        tx_modbuspacket[6] = reg_count * 2; /* Number of databytes to follow */
        body_size = 7;
    }
    else
    {
        modbus_debug("Unknown function.");
        return false;
    }

    /* If larger sizes than type u32 and i32 are required, this will
     * need to be changed. Mainly do this so floats can be set */
    uint32_t value32;
    if (type == MODBUS_REG_TYPE_I16 ||
        type == MODBUS_REG_TYPE_I32 )
    {
        int32_t valuei32 = value;
        value32 = *((uint32_t*)&valuei32);
    }
    else if (type == MODBUS_REG_TYPE_U16 ||
             type == MODBUS_REG_TYPE_U32 )
    {
        value32 = value;
    }
    else if (type == MODBUS_REG_TYPE_FLOAT)
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstrict-aliasing"
        value32 = *((uint32_t*)&value);
        #pragma GCC diagnostic pop
    }
    else
    {
        modbus_debug("Unknown type for converting value. (%d)", type);
        return false;
    }

    if (!_modbus_append_value(&tx_modbuspacket[body_size], MODBUS_PACKET_BUF_SIZ - body_size, type, byte_order, word_order, &value32, &body_size))
    {
        modbus_debug("Failed to append modbus value.");
        return false;
    }

    if (func == MODBUS_WRITE_SINGLE_HOLDING_FUNC)
    {
        _modbus_reg_set_expected.unit_id        = unit_id;
        _modbus_reg_set_expected.reg_addr       = reg_addr;
        _modbus_reg_set_expected.value          = value32;
    }
    else if (func == MODBUS_WRITE_MULTIPLE_HOLDING_FUNC)
    {
        _modbus_reg_set_expected.unit_id        = unit_id;
        _modbus_reg_set_expected.reg_addr       = reg_addr;
        _modbus_reg_set_expected.num_written    = reg_count;
    }
    else
    {
        modbus_debug("Undefined modbus function for setting expected.");
        return false;
    }


    uint16_t crc = modbus_crc(tx_modbuspacket, body_size);
    tx_modbuspacket[body_size++] = crc & 0xFF;
    tx_modbuspacket[body_size++] = crc >> 8;

    modbus_debug("Modbus packet (%u):", body_size);
    log_debug_data(DEBUG_MODBUS, tx_modbuspacket, body_size);

    modbus_want_rx = true;
    modbus_cur_send_time = get_since_boot_ms();
    if (modbus_bus->binary_protocol)
    {
        uart_ring_out(EXT_UART, (char[]){MODBUS_BIN_START}, 1);
        tx_modbuspacket[body_size++] = MODBUS_BIN_STOP;
    }
    uart_ring_out(EXT_UART, (char*)tx_modbuspacket, body_size);
    return true;
}


bool modbus_start_read(modbus_reg_t * reg)
{
    modbus_dev_t * dev = modbus_reg_get_dev(reg);

    if (!dev ||
        !(reg->func == MODBUS_READ_HOLDING_FUNC || reg->func == MODBUS_READ_INPUT_FUNC) ||
        !(reg->type == MODBUS_REG_TYPE_U16  ||
          reg->type == MODBUS_REG_TYPE_I16  ||
          reg->type == MODBUS_REG_TYPE_U32  ||
          reg->type == MODBUS_REG_TYPE_I32  ||
          reg->type == MODBUS_REG_TYPE_FLOAT))
    {
        log_error("Modbus register \"%."STR(MODBUS_NAME_LEN)"s\" unable to start read.", reg->name);
        return false;
    }

    if (ring_buf_is_full(&_message_queue))
    {
        if (modbus_want_rx)
        {
            uint32_t delta = since_boot_delta(get_since_boot_ms(), modbus_cur_send_time);
            if (delta < MODBUS_RESP_TIMEOUT_MS)
            {
                modbus_debug("No slot free.. sending to fast?? (%u/%u)", (unsigned)(ring_buf_get_pending(&_message_queue)/sizeof(modbus_reg_t*)), (unsigned)(_message_queue.size/sizeof(modbus_reg_t*)) );
                return false;
            }
        }
        else modbus_debug("No slot free, but not waiting?.. comms??");

        modbus_debug("Previous comms issue. Restarting slots.");
        ring_buf_clear(&_message_queue);

        modbus_read_last_good = 0;
        modbus_cur_send_time = 0;
        modbus_want_rx = false;
    }

    if (!ring_buf_add_data(&_message_queue, &reg, sizeof(modbus_reg_t*)))
    {
        log_error("Modbus queue error");
        ring_buf_clear(&_message_queue);
        return false;
    }

    reg->value_state = MB_REG_WAITING;

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
    ring_buf_clear(ring);

    modbus_retransmit_count++;
    if (modbus_retransmit_count < MODBUS_MAX_RETRANSMITS)
        _modbus_next_message();
    else
    {
        modbus_reg_t * current_reg = NULL;

        modbus_debug("Dropping message in queue.");
        modbus_retransmit_count = 0;

        if (ring_buf_read(&_message_queue, (char*)&current_reg, sizeof(current_reg)) != sizeof(current_reg) || current_reg == NULL)
        {
            modbus_debug("Failed to drop message, dropping all messages.");
            ring_buf_clear(&_message_queue);
        }
    }

    return true;
}

static void _modbus_reg_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order);

void modbus_uart_ring_in_process(ring_buf_t * ring)
{
    static unsigned _header_size = 0;
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

    if (modbus_requires_echo_removal() && _echo_bytes)
    {
        unsigned discarded = ring_buf_discard(ring, _echo_bytes);
        _echo_bytes -= discarded;
        if (!_echo_bytes)
            modbus_debug("Echo drained.");
        return;
    }

    if (!modbuspacket_len)
    {
        if ((!modbus_bus->binary_protocol && len > 2) || (modbus_bus->binary_protocol && len > 3))
        {
            _header_size = 0;
            modbus_debug("Starting reply read with : %u", len);

            modbus_read_timing_init = 0;
            ring_buf_read(ring, (char*)modbuspacket, 1);

            if (!modbuspacket[0])
            {
                modbus_debug("Zero start");
                return;
            }

            if (modbus_bus->binary_protocol)
            {
                if (modbuspacket[0] != MODBUS_BIN_START)
                {
                    modbus_debug("Not binary frame start.");
                    return;
                }
                ring_buf_read(ring, (char*)modbuspacket, 1);
                len-= 1;
                modbus_debug("Binary prototype read, %u left", len);
            }

            ring_buf_read(ring, (char*)modbuspacket + 1, 1);
            uint8_t func = modbuspacket[1];

            len -= 2; /* We wait for 3 though as that's the longest header we deal with. */
            modbus_debug("Reply, Address:%"PRIu8" Function: %"PRIu8, modbuspacket[0], func);
            if (func == MODBUS_READ_HOLDING_FUNC)
            {
                ring_buf_read(ring, (char*)modbuspacket + 2, 1);
                modbuspacket_len = modbuspacket[2] + 2 /* result data and crc*/;
                _header_size = 3;
            }
            else if (func == MODBUS_READ_INPUT_FUNC)
            {
                ring_buf_read(ring, (char*)modbuspacket + 2, 1);
                modbuspacket_len = modbuspacket[2] + 2 /* result data and crc*/;
                _header_size = 3;
            }
            else if (func == MODBUS_WRITE_SINGLE_HOLDING_FUNC)
            {
                /*We waited an extra byte more than required before starting the header as body is fixed length here.*/
                modbuspacket_len = 6; /* register and value written */
                _header_size = 2;
            }
            else if (func == MODBUS_WRITE_MULTIPLE_HOLDING_FUNC)
            {
                /*We waited an extra byte more than required before starting the header as body is fixed length here.*/
                modbuspacket_len = 6; /* first register and number of registes written. */
                _header_size = 2;
            }
            else if ((func & MODBUS_ERROR_MASK) == MODBUS_ERROR_MASK)
            {
                /*We waited an extra byte more than required before starting the header as body is fixed length here.*/
                modbuspacket_len = 3; /* Exception code and crc*/
                _header_size = 2;
            }

            if (!_header_size)
            {
                modbuspacket_len = 0;
                modbus_debug("Bad function");
                return;
            }

            modbus_debug("Reply type length : %u", modbuspacket_len);
            if (modbus_bus->binary_protocol)
            {
                modbuspacket_len++;
                modbus_debug("Binary prototype extra 1 byte required.");
            }

            modbus_read_timing_init = get_since_boot_ms();
            modbus_debug("header received, timer started at:%"PRIu32" body:%u/%u", modbus_read_timing_init, len, modbuspacket_len);
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

    ring_buf_read(ring, (char*)modbuspacket + _header_size, modbuspacket_len);

    if (modbus_bus->binary_protocol)
    {
        if (modbuspacket[modbuspacket_len + _header_size - 1] != MODBUS_BIN_STOP)
        {
            modbus_debug("Not binary frame stopped, discarded.");
            return;
        }
        modbuspacket_len--;
    }

    // Now include the header too.
    modbuspacket_len += _header_size;

    log_debug_data(DEBUG_MODBUS, modbuspacket, modbuspacket_len);

    uint16_t crc = modbus_crc(modbuspacket, modbuspacket_len - 2);

    if ( (modbuspacket[modbuspacket_len-1] != (crc >> 8)) ||
         (modbuspacket[modbuspacket_len-2] != (crc & 0xFF)) )
    {
        modbus_debug("Bad CRC");
        modbuspacket_len = 0;
        modbus_want_rx = false;
        return;
    }

    modbus_debug("Good CRC");
    modbuspacket_len = 0;
    modbus_want_rx = false;

    uint8_t unit_id = modbuspacket[0];
    uint8_t func    = modbuspacket[1];
    if (func == MODBUS_WRITE_SINGLE_HOLDING_FUNC)
    {
        if (unit_id != _modbus_reg_set_expected.unit_id)
        {
            _modbus_reg_set_expected.not_done = false;
            _modbus_reg_set_expected.passfail = false;
            modbus_debug("Unexpected unit id (0x%02"PRIX8" != 0x%02"PRIX8").", unit_id, _modbus_reg_set_expected.unit_id);
            return;
        }
        uint16_t reg_addr = (modbuspacket[2] << 8) | modbuspacket[3];
        if (reg_addr != _modbus_reg_set_expected.reg_addr)
        {
            _modbus_reg_set_expected.not_done = false;
            _modbus_reg_set_expected.passfail = false;
            modbus_debug("Unexpected register address (0x%04"PRIX16" != 0x%04"PRIX16").", reg_addr, _modbus_reg_set_expected.reg_addr);
            return;
        }
        uint16_t value = (modbuspacket[4] << 8) | modbuspacket[5];
        if (value != _modbus_reg_set_expected.value)
        {
            _modbus_reg_set_expected.not_done = false;
            _modbus_reg_set_expected.passfail = false;
            modbus_debug("Unexpected value (0x%04"PRIX16" != 04%"PRIX16").", value, _modbus_reg_set_expected.value);
            return;
        }
        modbus_debug("Received acknowledgement.");
        _modbus_reg_set_expected.not_done = false;
        _modbus_reg_set_expected.passfail = true;
        return;
    }
    else if (func == MODBUS_WRITE_MULTIPLE_HOLDING_FUNC)
    {
        if (unit_id != _modbus_reg_set_expected.unit_id)
        {
            _modbus_reg_set_expected.not_done = false;
            _modbus_reg_set_expected.passfail = false;
            modbus_debug("Unexpected unit id (0x%02"PRIX8" != 0x%02"PRIX8").", unit_id, _modbus_reg_set_expected.unit_id);
            return;
        }
        uint16_t reg_addr = (modbuspacket[2] << 8) | modbuspacket[3];
        if (reg_addr != _modbus_reg_set_expected.reg_addr)
        {
            _modbus_reg_set_expected.not_done = false;
            _modbus_reg_set_expected.passfail = false;
            modbus_debug("Unexpected register address (0x%04"PRIX16" != 0x%04"PRIX16").", reg_addr, _modbus_reg_set_expected.reg_addr);
            return;
        }
        uint16_t num_written = (modbuspacket[4] << 8) | modbuspacket[5];
        if (num_written != _modbus_reg_set_expected.num_written)
        {
            _modbus_reg_set_expected.not_done = false;
            _modbus_reg_set_expected.passfail = false;
            modbus_debug("Unexpected num_written (0x%04"PRIX16" != 04%"PRIX16").", num_written, _modbus_reg_set_expected.num_written);
            return;
        }
        modbus_debug("Received acknowledgement.");
        _modbus_reg_set_expected.not_done = false;
        _modbus_reg_set_expected.passfail = true;
        return;
    }

    // Good or bad, we think we have the whole message for current register, so remove it from queue.
    modbus_reg_t * current_reg = NULL;

    if (ring_buf_read(&_message_queue, (char*)&current_reg, sizeof(current_reg)) != sizeof(current_reg) || current_reg == NULL)
    {
        log_error("Modbus comms issues!");
        return;
    }

    if (current_reg->value_state != MB_REG_WAITING)
        modbus_debug("Reg :%."STR(MODBUS_NAME_LEN)"s not waiting!", current_reg->name);

    current_reg->value_state = MB_REG_INVALID;

    modbus_read_last_good = get_since_boot_ms();

    modbus_retransmit_count = 0;

    if ((modbuspacket[1] == (MODBUS_READ_HOLDING_FUNC | MODBUS_ERROR_MASK)) ||
        (modbuspacket[1] == (MODBUS_READ_INPUT_FUNC | MODBUS_ERROR_MASK)))
    {
        modbus_debug("Exception: 0x%02"PRIx8, modbuspacket[2]);
        return;
    }

    modbus_dev_t * dev = modbus_reg_get_dev(current_reg);

    if (dev->unit_id != modbuspacket[0])
    {
        log_error("Modbus comms issues!");
        return;
    }

    _modbus_reg_cb(current_reg, modbuspacket + 3, modbuspacket[2], dev->byte_order, dev->word_order);
}


static bool _modbus_data_to_u16(uint16_t* value, const uint8_t* data, uint8_t size, modbus_byte_orders_t byte_order)
{
    if (!value)
    {
        modbus_debug("Handed NULL pointer.");
        return false;
    }
    if (size != 2)
    {
        modbus_debug("Not given array of size 2.");
        return false;
    }
    switch (byte_order)
    {
        case MODBUS_BYTE_ORDER_MSB:
            *value = ((data[0] << 8) | data[1]);
            return true;
        case MODBUS_BYTE_ORDER_LSB:
            *value = ((data[1] << 8) | data[0]);
            return true;
        default:
            modbus_debug("Unknown byte order.");
            return false;
    }
}


static bool _modbus_data_to_u32(uint32_t* value, uint8_t* data, uint8_t size, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (!value)
    {
        modbus_debug("Handed NULL pointer.");
        return false;
    }
    if (size != 4)
    {
        modbus_debug("Not given array size of 4.");
        return false;
    }
    uint16_t word_1, word_2;
    if (!_modbus_data_to_u16(&word_1, data  , 2, byte_order))
        return false;
    if (!_modbus_data_to_u16(&word_2, data+2, 2, byte_order))
        return false;
    switch (word_order)
    {
        case MODBUS_WORD_ORDER_MSW:
            *value = (word_1 << 16) | word_2;
            return true;
        case MODBUS_WORD_ORDER_LSW:
            *value = (word_2 << 16) | word_1;
            return true;
        default:
            modbus_debug("Unknown word order.");
            return false;
    }
}


static void _modbus_reg_set(modbus_reg_t * reg, uint32_t v)
{
    reg->value_data = v;
    reg->value_state = MB_REG_READY;
}

static void _modbus_reg_u16_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order)
{
    if (size != 2)
        return;
    uint16_t v;
    if (!_modbus_data_to_u16(&v, data, size, byte_order))
        return;
    _modbus_reg_set(reg, v);
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s U16:%"PRIu16, reg->name, v);
}

static void _modbus_reg_i16_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order)
{
    if (size != 2)
        return;
    int16_t v;
    if (!_modbus_data_to_u16((uint16_t*)&v, data, size, byte_order))
        return;
    _modbus_reg_set(reg, v);
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s I16:%"PRIi16, reg->name, v);
}

static void _modbus_reg_u32_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (size != 4)
        return;
    uint32_t v;
    if (!_modbus_data_to_u32(&v, data, size, byte_order, word_order))
        return;
    _modbus_reg_set(reg, v);
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s U32:%"PRIu32, reg->name, v);
}

static void _modbus_reg_i32_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (size != 4)
        return;
    int32_t v;
    if (!_modbus_data_to_u32((uint32_t*)&v, data, size, byte_order, word_order))
        return;
    _modbus_reg_set(reg, v);
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s I32:%"PRIi32, reg->name, v);
}

static void _modbus_reg_float_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (size != 4)
        return;
    uint32_t v=0;
    if (!_modbus_data_to_u32(&v, data, size, byte_order, word_order))
        return;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    modbus_debug("reg:%."STR(MODBUS_NAME_LEN)"s F32:%f", reg->name, *(float*)&v);
    #pragma GCC diagnostic pop
    _modbus_reg_set(reg, v);
}

static void _modbus_reg_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    switch(reg->type)
    {
        case MODBUS_REG_TYPE_U16:    _modbus_reg_u16_cb(reg, data, size, byte_order); break;
        case MODBUS_REG_TYPE_I16:    _modbus_reg_i16_cb(reg, data, size, byte_order); break;
        case MODBUS_REG_TYPE_U32:    _modbus_reg_u32_cb(reg, data, size, byte_order, word_order); break;
        case MODBUS_REG_TYPE_I32:    _modbus_reg_i32_cb(reg, data, size, byte_order, word_order); break;
        case MODBUS_REG_TYPE_FLOAT:  _modbus_reg_float_cb(reg, data, size, byte_order, word_order); break;
        default: log_error("Unknown modbus reg type."); break;
    }
}


bool modbus_uart_ring_do_out_drain(ring_buf_t * ring)
{
    unsigned len = ring_buf_get_pending(ring);

    static bool rs485_transmitting = false;
    static uint32_t rs485_start_transmitting = 0;

    if (!len)
    {
        if (rs485_transmitting && uart_is_tx_empty(EXT_UART))
        {
            static bool rs485_transmit_stopping = false;
            static uint32_t rs485_stop_transmitting = 0;
            if (!rs485_transmit_stopping)
            {
                modbus_debug("Sending complete, delay %"PRIu32"ms", modbus_stop_delay());
                rs485_stop_transmitting = get_since_boot_ms();
                rs485_transmit_stopping = true;
            }
            else if (since_boot_delta(get_since_boot_ms(), rs485_stop_transmitting) > modbus_stop_delay())
            {
                rs485_transmitting = false;
                rs485_transmit_stopping = false;
                platform_set_rs485_mode(false);
            }
        }
        return false;
    }

    if (!rs485_transmitting)
    {
        rs485_transmitting = true;
        platform_set_rs485_mode(true);
        rs485_start_transmitting = get_since_boot_ms();
        modbus_debug("Data to send, delay %"PRIu32"ms", modbus_start_delay());
        return false;
    }
    else
    {
        if (since_boot_delta(get_since_boot_ms(), rs485_start_transmitting) < modbus_start_delay())
            return false;
    }

    modbus_debug("Sending %u", len);
    return true;
}


void modbus_init(void)
{
    modbus_bus_t * bus = &persist_data.model_config.modbus_bus;
    modbus_bus_init(bus);

    uart_resetup(EXT_UART,
                 bus->baudrate,
                 bus->databits,
                 bus->parity,
                 bus->stopbits, &uart_cmd_ctx);

    modbus_bus->binary_protocol = bus->binary_protocol;

    _modbus_setup_delays(bus->baudrate,
                         bus->databits,
                         bus->parity,
                         bus->stopbits);
}


static command_response_t _modbus_setup_cb(char *args, cmd_ctx_t * ctx)
{
    /*<BIN/RTU> <SPEED> <BITS><PARITY><STOP>
     * EXAMPLE: RTU 115200 8N1
     */
    return modbus_setup_from_str(args, ctx) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _modbus_log_cb(char* args, cmd_ctx_t * ctx)
{
    modbus_print(ctx);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* modbus_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "mb_setup",     "Change Modbus comms",      _modbus_setup_cb               , false , NULL },
        { "mb_log",       "Show modbus setup",        _modbus_log_cb                 , false , NULL },
    };
    return modbus_add_mem_commands(add_commands(tail, cmds, ARRAY_SIZE(cmds)));
}
