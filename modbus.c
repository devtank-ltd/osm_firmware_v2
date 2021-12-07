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
#include "sys_time.h"
#include "cmd.h"
#include "persist_config.h"

#define MODBUS_RESP_TIMEOUT_MS 2000
#define MODBUS_SENT_TIMEOUT_MS 2000

#define MODBUS_ERROR_MASK 0x80

#define MODBUS_BIN_START '{'
#define MODBUS_BIN_STOP '}'

#define MODBUS_BLOB_VERSION 1
#define MODBUS_MAX_DEV 4

typedef void (*modbus_reg_cb)(modbus_reg_t * reg, uint8_t * data, uint8_t size);


/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                     <             PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)

    Also good source : http://www.simplymodbus.ca
*/

#define MAX_MODBUS_PACKET_SIZE    127

typedef struct
{
    uint8_t version;
    uint8_t max_dev_num;
    uint8_t max_reg_num;
    uint8_t _;
    uint32_t __;
    uint32_t size;
    uint32_t used;
} __attribute__((__packed__)) modbus_blob_header_t;

struct modbus_reg_t
{
    char              name[MODBUS_NAME_LEN];
    uint32_t          class_data_a; /* what ever child class wants */
    uint8_t           type; /*modbus_reg_type_t*/
    uint8_t           func;
    uint16_t          class_data_b; /* what ever child class wants */
    uint16_t          device_offset; /* Offset to device */
    uint16_t          reg_addr;
} __attribute__((__packed__)) ;

struct modbus_dev_t
{
    char           name[MODBUS_NAME_LEN];
    uint8_t        slave_id;
    uint8_t        reg_num;
    uint16_t       _; /* pad.*/
    uint32_t       regs[MODBUS_DEV_REGS]; /* Offsets to regsiters */
} __attribute__((__packed__)) ;


static uint8_t * modbus_data = NULL;
static uint8_t * modbus_data_pos = NULL;

static modbus_dev_t * modbus_devices = NULL;

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


char * modbus_reg_type_get_str(modbus_reg_type_t type)
{
    switch(type)
    {
        case MODBUS_REG_TYPE_U16:    return "U16"; break;
        case MODBUS_REG_TYPE_U32:    return "U32"; break;
        case MODBUS_REG_TYPE_FLOAT:  return "F"; break;
        default:
            break;
    }
    return NULL;
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

    unsigned reg_count;

    switch (reg->type)
    {
        case MODBUS_REG_TYPE_U16    : reg_count = 1; break;
        case MODBUS_REG_TYPE_U32    : reg_count = 2; break;
        case MODBUS_REG_TYPE_FLOAT  : reg_count = 2; break;
        default:
            return false;
    }

    modbus_dev_t * dev = modbus_reg_get_dev(reg);

    modbus_debug("Reading %"PRIu8" of 0x%"PRIx8":0x%"PRIx8 , reg_count, dev->slave_id, reg->reg_addr);

    /* ADU Header (Application Data Unit) */
    modbuspacket[0] = dev->slave_id;
    /* ====================================== */
    /* PDU payload (Protocol Data Unit) */
    modbuspacket[1] = MODBUS_READ_HOLDING_FUNC; /*Holding*/
    modbuspacket[2] = reg->reg_addr >> 8;   /*Register read address */
    modbuspacket[3] = reg->reg_addr & 0xFF;
    modbuspacket[4] = reg_count >> 8; /*Register read count */
    modbuspacket[5] = reg_count & 0xFF;
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

    /* All current types use this as is_valid. */
    reg->class_data_b = 0;

    current_reg = reg;
    modbus_sent_timing_init = since_boot_ms;
    return true;
}

static void _modbus_reg_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size);

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

    if (current_reg)
    {
        modbus_dev_t * dev = modbus_reg_get_dev(current_reg);

        if (dev->slave_id == modbuspacket[0])
        {
            modbus_reg_t * reg = current_reg;
            current_reg = NULL;
            _modbus_reg_cb(reg, modbuspacket + 3, modbuspacket[2]);
            return;
        }
    }
    modbus_debug("Unexpected packet");
}


static void _modbus_reg_set(modbus_reg_t * reg, uint32_t v)
{
    reg->class_data_a = v;
    reg->class_data_b = true;
}

static bool _modbus_reg_is_valid(modbus_reg_t * reg)
{
    return reg->class_data_b;
}

static uint32_t _modbus_reg_get(modbus_reg_t * reg)
{
    return reg->class_data_a;
}


static void _modbus_reg_u16_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    if (size != 2)
        return;
    uint16_t v = data[1] << 8 | data[0];
    _modbus_reg_set(reg, v);
    modbus_debug("U16:%"PRIu16, v);
}

static void _modbus_reg_u32_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    if (size != 4)
        return;
    uint32_t v = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    _modbus_reg_set(reg, v);
    modbus_debug("U32:%"PRIu32, v);
}

static void _modbus_reg_float_cb(modbus_reg_t * reg, uint8_t * data, uint8_t size)
{
    if (size != 4)
        return;
    uint32_t v = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    float f = *(float*)&v;
    #pragma GCC diagnostic pop
    _modbus_reg_set(reg, v);
    modbus_debug("F32:%f", f);
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


static modbus_reg_t * _modbus_dev_get_reg_by_index(modbus_dev_t * dev, unsigned index)
{
    return (modbus_reg_t*)(modbus_data + dev->regs[index]);
}


static uint64_t _get_id_of_name(char name[8])
{
    return *(uint64_t*)name;
}


static modbus_reg_t * _modbus_dev_get_reg_by_id(modbus_dev_t * dev, uint64_t id)
{
    for(unsigned n = 0; n < dev->reg_num; n++)
    {
        modbus_reg_t * reg = _modbus_dev_get_reg_by_index(dev, n);
        if (id == _get_id_of_name(reg->name))
            return reg;
    }
    return NULL;
}


void           modbus_log()
{
    {
        unsigned speed;
        uint8_t databits;
        uart_parity_t parity;
        uart_stop_bits_t stop;

        uart_get_setup(RS485_UART, &speed, &databits, &parity, &stop);

        log_out("Modbus @ %s %u %u%c%s", (do_binary_framing)?"BIN":"RTU", speed, databits, uart_parity_as_char(parity), uart_stop_bits_as_str(stop));
    }

    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = modbus_devices + n;
        if (dev->name[0])
        {
            log_out("- Device - 0x%"PRIx16" \"%s\"", dev->slave_id, dev->name);
            for (unsigned i = 0; i < dev->reg_num; i++)
            {
                modbus_reg_t * reg = _modbus_dev_get_reg_by_index(dev, i);
                log_out("  - Reg - 0x%"PRIx16" \"%s\" %s", reg->reg_addr, reg->name, modbus_reg_type_get_str(reg->type));
            }
        }
    }
}


unsigned      modbus_get_device_count(void)
{
    unsigned r = 0;
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
        if (modbus_devices[n].name[0])
            r++;
    return r;
}


modbus_dev_t * modbus_get_device(unsigned index)
{
    if (index >= MODBUS_MAX_DEV)
        return NULL;
    return &modbus_devices[index];
}

modbus_dev_t * modbus_get_device_by_id(unsigned slave_id)
{
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = &modbus_devices[n];
        if (dev->name[0] && dev->slave_id == slave_id)
            return &modbus_devices[n];
    }
    return NULL;
}


modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name)
{
    if (!dev || !name)
        return NULL;
    uint64_t id = 0;
    memcpy(&id, name, strlen(name));
    return _modbus_dev_get_reg_by_id(dev, id);
}


char *         modbus_dev_get_name(modbus_dev_t * dev)
{
    if (!dev)
        return NULL;
    return dev->name;
}


uint16_t       modbus_dev_get_slave_id(modbus_dev_t * dev)
{
    if (!dev)
        return 0;
    return dev->slave_id;
}


modbus_reg_t* modbus_get_reg(char * name)
{
    if (!name)
        return NULL;
    uint64_t id = 0;
    memcpy(&id, name, strlen(name));
    for(unsigned n = 0; n < modbus_get_device_count(); n++)
    {
        modbus_reg_t * reg = _modbus_dev_get_reg_by_id(modbus_get_device(n), id);
        if (reg)
            return reg;
    }
    return NULL;
}


modbus_dev_t * modbus_add_device(unsigned slave_id, char *name)
{
    if (!name || !slave_id)
        return NULL;

    unsigned len = strlen(name) + 1;

    if (len > MODBUS_NAME_LEN)
        return NULL;

    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = &modbus_devices[n];
        if (!dev->name[0])
        {
            memcpy(dev->name, name, len);
            dev->slave_id = slave_id;
            dev->reg_num = 0;
            modbus_debug("Added device 0x%"PRIx16" \"%s\"", slave_id, name);
            return dev;
        }
    }

    return NULL;
}


void           modbus_config_wipe(void)
{
    modbus_blob_header_t * header = (modbus_blob_header_t*)modbus_data;

    memset(modbus_devices, 0, sizeof(modbus_dev_t) * MODBUS_MAX_DEV);
    modbus_data_pos = modbus_data + sizeof(modbus_blob_header_t) + (sizeof(modbus_dev_t) * MODBUS_MAX_DEV);

    header->version = MODBUS_BLOB_VERSION;
    header->max_dev_num = MODBUS_MAX_DEV;
    header->max_reg_num = MODBUS_DEV_REGS;
    header->size = MODBUS_MEMORY_SIZE;
    header->used = ((uintptr_t)modbus_data_pos) - ((uintptr_t)modbus_data);

    modbus_debug("%"PRIu32" remaining space", (header->size - header->used));
}


bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr)
{
    if (!dev || !name)
        return false;

    if (dev->reg_num == MODBUS_DEV_REGS)
    {
        modbus_debug("Dev has max regs.");
        return false;
    }

    if (modbus_get_reg(name) != NULL)
    {
        modbus_debug("Name used.");
        return false;
    }

    if (func != MODBUS_READ_HOLDING_FUNC)
    {
        modbus_debug("Unsupported func");
        return false;
    }

    unsigned name_len = strlen(name) + 1;

    if (name_len > MODBUS_NAME_LEN)
    {
        modbus_debug("Name too long");
        return false;
    }

    unsigned size = sizeof(modbus_reg_t);

    size = ALIGN_16(size);

    if ((modbus_data_pos + size) > (modbus_data + MODBUS_MEMORY_SIZE))
    {
        modbus_debug("Not enough memory to add reg.");
        return false;
    }

    modbus_reg_t * dst_reg = (modbus_reg_t*)modbus_data_pos;

    modbus_data_pos += size;

    modbus_blob_header_t * header = (modbus_blob_header_t*)modbus_data;

    header->used = ((uintptr_t)modbus_data_pos) - ((uintptr_t)modbus_data);
    modbus_debug("%"PRIu32" remaining space", (header->size - header->used));

    memcpy(dst_reg->name, name, name_len);
    dst_reg->reg_addr  = reg_addr;
    dst_reg->type      = type;
    dst_reg->func      = func;
    dst_reg->device_offset = ((intptr_t)dev) - ((intptr_t)modbus_data);

    dev->regs[dev->reg_num++] = ((intptr_t)dst_reg) - ((intptr_t)modbus_data);

    /* For the current types, this start as zero. */
    dst_reg->class_data_a = 0;
    dst_reg->class_data_b = 0;

    return true;
}


unsigned       modbus_dev_get_reg_num(modbus_dev_t * dev)
{
    if (!dev)
        return 0;
    return dev->reg_num;
}


modbus_reg_t * modbus_dev_get_reg(modbus_dev_t * dev, unsigned index)
{
    if (!dev)
        return NULL;
    if (index >= dev->reg_num)
        return NULL;
    return _modbus_dev_get_reg_by_index(dev, index);
}


modbus_reg_type_t modbus_reg_get_type(modbus_reg_t * reg)
{
    if (!reg)
        return MODBUS_REG_TYPE_INVALID;
    return reg->type;
}


bool              modbus_reg_get_u16(modbus_reg_t * reg, uint16_t * value)
{
    if (!reg || !value)
        return false;
    if (!_modbus_reg_is_valid(reg))
        return false;
    uint32_t v = _modbus_reg_get(reg);
    *value = ((uint16_t)v)&0xFFFF;
    return true;
}


bool              modbus_reg_get_u32(modbus_reg_t * reg, uint32_t * value)
{
    if (!reg || !value)
        return false;
    if (!_modbus_reg_is_valid(reg))
        return false;
    *value = _modbus_reg_get(reg);
    return true;
}


bool              modbus_reg_get_float(modbus_reg_t * reg, float * value)
{
    if (!reg || !value)
        return false;
    if (!_modbus_reg_is_valid(reg))
        return false;
    uint32_t v = _modbus_reg_get(reg);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    *value = *(float*)&v;
    #pragma GCC diagnostic pop

    return true;
}


modbus_dev_t    * modbus_reg_get_dev(modbus_reg_t * reg)
{
    if (!reg)
        return NULL;
    return (modbus_dev_t*)(modbus_data + reg->device_offset);
}


void modbus_save()
{
    persist_commit_modbus_data();
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

    modbus_data    = persist_get_modbus_data();

    modbus_blob_header_t * header = (modbus_blob_header_t*)modbus_data;

    modbus_devices = (modbus_dev_t*)(modbus_data + sizeof(modbus_blob_header_t));

    if (header->version == MODBUS_BLOB_VERSION &&
        header->max_dev_num == MODBUS_MAX_DEV      &&
        header->max_reg_num == MODBUS_DEV_REGS     &&
        header->size <= MODBUS_MEMORY_SIZE)
    {
        log_sys_debug("Loaded modbus defs");
        modbus_data_pos = modbus_data + header->used;
        modbus_debug("%"PRIu32" remaining space", (header->size - header->used));
    }
    else
    {
        log_sys_debug("Failed to load modbus defs");
        modbus_config_wipe();
    }
}
