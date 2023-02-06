#include <inttypes.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "modbus.h"
#include "modbus_measurements.h"
#include "modbus_mem.h"
#include "uart_rings.h"
#include "uarts.h"
#include "common.h"
#include "cmd.h"
#include "persist_config.h"
#include "platform.h"
#include "pinmap.h"

#define MODBUS_RESP_TIMEOUT_MS 2000
#define MODBUS_SENT_TIMEOUT_MS 2000
#define MODBUS_TX_GAP_MS        100

#define MODBUS_ERROR_MASK 0x80

#define MODBUS_BIN_START '{'
#define MODBUS_BIN_STOP '}'

#define MODBUS_SLOTS  (MODBUS_BLOCKS / MODBUS_BLOCK_SIZE)

#define MODBUS_MAX_RETRANSMITS 10

/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                     <             PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)

    Also good source : http://www.simplymodbus.ca
*/

#define MAX_MODBUS_PACKET_SIZE    127


static uint8_t modbuspacket[MAX_MODBUS_PACKET_SIZE];
static uint8_t tx_modbuspacket[10];

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
    modbus_debug("Modbus @ %s %u %u%c%s", (modbus_bus->binary_protocol)?"BIN":"RTU", speed, databits, uart_parity_as_char(parity), uart_stop_bits_as_str(stop));

    modbus_bus->baudrate    = speed;
    modbus_bus->databits    = databits;
    modbus_bus->parity      = parity;
    modbus_bus->stopbits    = stop;
}


void modbus_setup(unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop, bool binary_framing)
{
    uart_resetup(EXT_UART, speed, databits, parity, stop);

    modbus_bus->binary_protocol = binary_framing;

    _modbus_setup_delays(speed, databits, parity, stop);
}



bool modbus_setup_from_str(char * str)
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
        log_error("Unknown modbus protocol.");
        return false;
    }

    pos+=3;

    if (!uart_resetup_str(EXT_UART, skip_space(pos)))
        return false;

    unsigned speed;
    uint8_t databits;
    uart_parity_t parity;
    uart_stop_bits_t stop;

    uart_get_setup(EXT_UART, &speed, &databits, &parity, &stop);

    modbus_bus->binary_protocol = binary_framing;

    _modbus_setup_delays(speed, databits, parity, stop);

    return true;
}


bool modbus_add_dev_from_str(char* str)
{
    /*<unit_id> <LSB/MSB> <LSW/MSW> <name>
     * (name can only be 4 char long)
     * EXAMPLES:
     * 0x1 MSB MSW TEST
     */
    char * pos = skip_space(str);

    uint16_t unit_id = strtoul(pos, &pos, 16);
    pos = skip_space(pos);

    if (!(toupper(pos[1]) == 'S') &&
        !(toupper(pos[2]) == 'B'))
        goto bad_exit;
    modbus_byte_orders_t byte_order;
    if (toupper(pos[0]) == 'L')
        byte_order = MODBUS_BYTE_ORDER_LSB;
    else if (toupper(pos[0]) == 'M')
        byte_order = MODBUS_BYTE_ORDER_MSB;
    else
        goto bad_exit;
    pos += 3;
    pos = skip_space(pos);

    if (!(toupper(pos[1]) == 'S') &&
        !(toupper(pos[2]) == 'W'))
        goto bad_exit;
    modbus_word_orders_t word_order;
    if (toupper(pos[0]) == 'L')
        word_order = MODBUS_WORD_ORDER_LSW;
    else if (toupper(pos[0]) == 'M')
        word_order = MODBUS_WORD_ORDER_MSW;
    else
        goto bad_exit;
    pos += 3;

    char * name = skip_space(pos);

    if (modbus_add_device(unit_id, name, byte_order, word_order))
        log_out("Added modbus device");
    else
        log_out("Failed to add modbus device.");
    return true;
bad_exit:
    modbus_debug("Unknown format.");
    return false;
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
    }
    else uart_ring_out(EXT_UART, (char*)tx_modbuspacket, 8); /* Frame is done with silence */

    reg->value_state = MB_REG_WAITING;
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
                modbus_debug("No slot free.. sending to fast??");
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
        if ((!modbus_bus->binary_protocol && len > 2) || (modbus_bus->binary_protocol && len > 3))
        {
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
                modbuspacket_len = 2; /* Exception code is in header, so just crc*/
            }
            else
            {
                modbus_debug("Bad function : %"PRIu8, func);
                return;
            }

            if (modbus_bus->binary_protocol)
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

    if (modbus_bus->binary_protocol)
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

    // Good or bad, we think we have the whole message for current register, so remove it from queue.
    modbus_reg_t * current_reg = NULL;

    if (ring_buf_read(&_message_queue, (char*)&current_reg, sizeof(current_reg)) != sizeof(current_reg) || current_reg == NULL)
    {
        log_error("Modbus comms issues!");
        modbuspacket_len = 0;
        modbus_want_rx = false;
        return;
    }

    if (current_reg->value_state != MB_REG_WAITING)
        modbus_debug("Reg :%."STR(MODBUS_NAME_LEN)"s not waiting!", current_reg->name);

    current_reg->value_state = MB_REG_INVALID;

    if ( (modbuspacket[modbuspacket_len-1] == (crc >> 8)) &&
         (modbuspacket[modbuspacket_len-2] == (crc & 0xFF)) )
    {
        modbus_debug("Bad CRC");
        modbuspacket_len = 0;
        modbus_want_rx = false;
        return;
    }

    modbus_debug("Good CRC");
    modbuspacket_len = 0;
    modbus_read_last_good = get_since_boot_ms();
    modbus_want_rx = false;

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
    uint32_t v;
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
    modbus_setup(bus->baudrate,
                 bus->databits,
                 bus->parity,
                 bus->stopbits,
                 bus->binary_protocol);
}


static command_response_t _modbus_setup_cb(char *args)
{
    /*<BIN/RTU> <SPEED> <BITS><PARITY><STOP>
     * EXAMPLE: RTU 115200 8N1
     */
    return modbus_setup_from_str(args) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _modbus_add_dev_cb(char * args)
{
    /*<unit_id> <LSB/MSB> <LSW/MSW> <name>
     * (name can only be 4 char long)
     * EXAMPLES:
     * 0x1 MSB MSW TEST
     */
    if (!modbus_add_dev_from_str(args))
    {
        log_out("<unit_id> <LSB/MSB> <LSW/MSW> <name>");
        return COMMAND_RESP_ERR;
    }
    return COMMAND_RESP_OK;
}


static command_response_t _modbus_add_reg_cb(char * args)
{
    /*<unit_id> <reg_addr> <modbus_func> <type> <name>
     * (name can only be 4 char long)
     * Only Modbus Function 3, Hold Read supported right now.
     * 0x1 0x16 3 F   T-Hz
     * 1 22 3 F       T-Hz
     * 0x2 0x30 3 U16 T-As
     * 0x2 0x32 3 U32 T-Vs
     */
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    uint16_t unit_id = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    uint16_t reg_addr = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    uint8_t func = strtoul(pos, &pos, 10);

    pos = skip_space(pos);

    modbus_reg_type_t type = modbus_reg_type_from_str(pos, (const char**)&pos);
    if (type == MODBUS_REG_TYPE_INVALID)
        return COMMAND_RESP_ERR;

    pos = skip_space(pos);

    char * name = pos;

    modbus_dev_t * dev = modbus_get_device_by_id(unit_id);
    if (!dev)
    {
        log_out("Unknown modbus device.");
        return COMMAND_RESP_ERR;
    }

    if (modbus_dev_add_reg(dev, name, type, func, reg_addr))
    {
        log_out("Added modbus reg %s", name);
        if (!modbus_measurement_add(modbus_dev_get_reg_by_name(dev, name)))
        {
            log_out("Failed to add modbus reg to measurements!");
            return COMMAND_RESP_ERR;
        }
        return COMMAND_RESP_OK;
    }
    log_out("Failed to add modbus reg.");
    return COMMAND_RESP_ERR;
}


static command_response_t _modbus_log_cb(char* args)
{
    modbus_log();
    return COMMAND_RESP_OK;
}


static command_response_t _modbus_measurement_del_reg_cb(char* args)
{
    return modbus_measurement_del_reg(args) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _modbus_measurement_del_dev_cb(char* args)
{
    return modbus_measurement_del_dev(args) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


struct cmd_link_t* modbus_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "mb_setup",     "Change Modbus comms",      _modbus_setup_cb               , false , NULL },
                                       { "mb_dev_add",   "Add modbus dev",           _modbus_add_dev_cb             , false , NULL },
                                       { "mb_reg_add",   "Add modbus reg",           _modbus_add_reg_cb             , false , NULL },
                                       { "mb_reg_del",   "Delete modbus reg",        _modbus_measurement_del_reg_cb , false , NULL },
                                       { "mb_dev_del",   "Delete modbus dev",        _modbus_measurement_del_dev_cb , false , NULL },
                                       { "mb_log",       "Show modbus setup",        _modbus_log_cb                 , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
