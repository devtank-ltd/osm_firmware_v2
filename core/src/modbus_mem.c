#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "modbus_mem.h"
#include "modbus.h"
#include "log.h"
#include "persist_config.h"
#include "pinmap.h"


modbus_bus_t * modbus_bus = NULL;


static uintptr_t _modbus_get_offset(void * ptr)
{
    return ((uintptr_t)ptr) - ((uintptr_t)modbus_bus);
}


static void * _modbus_get_from_offset(uintptr_t offset)
{
    return (void*)(((uintptr_t)modbus_bus) + offset);
}


static void _modbus_free(void * block)
{
    modbus_free_t * freed = block;

    memset(freed, 0, sizeof(modbus_free_t));

    if (modbus_bus->first_free_offset)
        freed->next_free_offset = modbus_bus->first_free_offset;

    modbus_bus->first_free_offset = _modbus_get_offset(freed);
}


static void* _modbus_allocate_block(void)
{
    if (!modbus_bus || !modbus_bus->first_free_offset)
    {
        modbus_debug("Modbus memory full.");
        return NULL;
    }

    modbus_free_t * freed = _modbus_get_from_offset(modbus_bus->first_free_offset);

    modbus_bus->first_free_offset = freed->next_free_offset;

    return freed;
}


static modbus_dev_t * _modbus_get_first_dev2(modbus_bus_t* bus)
{
    if (!bus || !bus->first_dev_offset)
        return NULL;

    return _modbus_get_from_offset(bus->first_dev_offset);
}


static modbus_dev_t * _modbus_get_first_dev(void)
{
    return _modbus_get_first_dev2(modbus_bus);
}


static modbus_dev_t * _modbus_get_next_dev(modbus_dev_t * dev)
{
    if (!dev || !dev->next_dev_offset)
        return NULL;

    return _modbus_get_from_offset(dev->next_dev_offset);
}


static modbus_reg_t * _modbus_get_first_reg(modbus_dev_t * dev)
{
    if (!dev || !dev->first_reg_offset)
        return NULL;

    return _modbus_get_from_offset(dev->first_reg_offset);
}


static modbus_reg_t * _modbus_get_next_reg(modbus_reg_t * reg)
{
    if (!reg || !reg->next_reg_offset)
        return NULL;

    return _modbus_get_from_offset(reg->next_reg_offset);
}


void modbus_log()
{
    if (!modbus_bus)
        return;

    log_out("Modbus @ %s %"PRIu32" %u%c%s", (modbus_bus->binary_protocol)?"BIN":"RTU", modbus_bus->baudrate, modbus_bus->databits, osm_uart_parity_as_char(modbus_bus->parity), osm_uart_stop_bits_as_str(modbus_bus->stopbits));

    modbus_dev_t * dev = _modbus_get_first_dev();
    while(dev)
    {
        char byte_char = 'M';
        char word_char = 'M';
        if (dev->byte_order == MODBUS_BYTE_ORDER_LSB)
            byte_char = 'L';
        if (dev->word_order == MODBUS_WORD_ORDER_LSW)
            word_char = 'L';
        log_out("- Device - 0x%"PRIx16" \"%."STR(MODBUS_NAME_LEN)"s\" %cSB %cSW", dev->unit_id, dev->name, byte_char, word_char);
        modbus_reg_t * reg = _modbus_get_first_reg(dev);
        while(reg)
        {
            log_out("  - Reg - 0x%"PRIx16" (F:%"PRIu8") \"%."STR(MODBUS_NAME_LEN)"s\" %s", reg->reg_addr, reg->func, reg->name, modbus_reg_type_get_str(reg->type));
            reg = _modbus_get_next_reg(reg);
        }
        dev = _modbus_get_next_dev(dev);
    }
}

modbus_reg_type_t modbus_reg_type_from_str(const char * type, const char ** pos)
{
    if (!type)
        return MODBUS_REG_TYPE_INVALID;

    bool is_signed;

    if (type[0] == 'F' && (type[1] == ' ' || type[1] == 0))
    {
        if (pos)
            *pos = type+2;
        return MODBUS_REG_TYPE_FLOAT;
    }

    else if (type[0] == 'U')
        is_signed = false;

    else if (type[0] == 'I')
        is_signed = true;

    else
    {
        log_out("Unknown modbus reg type.");
        return MODBUS_REG_TYPE_INVALID;
    }


    if (type[1] == '1' && type[2] == '6' && (type[3] == ' '|| type[3] == 0))
    {
        if (pos)
            *pos = type+3;
        return is_signed ? MODBUS_REG_TYPE_I16 : MODBUS_REG_TYPE_U16;
    }
    else if (type[1] == '3' && type[2] == '2' && (type[3] == ' '|| type[3] == 0))
    {
        if (pos)
            *pos = type+3;
        return is_signed ? MODBUS_REG_TYPE_I32 : MODBUS_REG_TYPE_U32;
    }
    log_out("Unknown modbus reg type.");
    return MODBUS_REG_TYPE_INVALID;
}


char * modbus_reg_type_get_str(modbus_reg_type_t type)
{
    switch(type)
    {
        case MODBUS_REG_TYPE_U16:    return "U16"; break;
        case MODBUS_REG_TYPE_I16:    return "I16"; break;
        case MODBUS_REG_TYPE_U32:    return "U32"; break;
        case MODBUS_REG_TYPE_I32:    return "I32"; break;
        case MODBUS_REG_TYPE_FLOAT:  return "F"; break;
        default:
            break;
    }
    return NULL;
}


static bool _modbus_reg_is_valid(modbus_reg_t * reg)
{
    return (reg->value_state == MB_REG_READY);
}


static uint32_t _modbus_reg_get(modbus_reg_t * reg)
{
    return reg->value_data;
}


modbus_dev_t * modbus_get_device_by_id(unsigned unit_id)
{
    modbus_dev_t * dev = _modbus_get_first_dev();
    while(dev)
    {
        if (dev->unit_id == unit_id)
            return dev;
        dev = _modbus_get_next_dev(dev);
    }
    return NULL;
}


static modbus_dev_t * _modbus_get_device_by_name(modbus_bus_t* bus, char * name)
{
    if (!name)
        return NULL;
    unsigned name_len = strlen(name);
    if (name_len > MODBUS_NAME_LEN)
        return NULL;
    modbus_dev_t * dev = _modbus_get_first_dev2(bus);
    while(dev)
    {
        if (strncmp(name, dev->name, MODBUS_NAME_LEN) == 0)
            return dev;
        dev = _modbus_get_next_dev(dev);
    }
    return NULL;
}


modbus_dev_t * modbus_get_device_by_name(char * name)
{
    return _modbus_get_device_by_name(modbus_bus, name);
}


modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name)
{
    if (!dev || !name)
        return NULL;

    unsigned name_len = strlen(name);
    if (name_len > MODBUS_NAME_LEN)
        return NULL;

    modbus_reg_t * reg = _modbus_get_first_reg(dev);
    while(reg)
    {
        if (strncmp(name, reg->name, MODBUS_NAME_LEN) == 0)
            return reg;
        reg = _modbus_get_next_reg(reg);
    }

    return NULL;
}


uint16_t       modbus_dev_get_unit_id(modbus_dev_t * dev)
{
    if (!dev)
        return 0;
    return dev->unit_id;
}


bool           modbus_for_each_dev(bool (exit_cb)(modbus_dev_t * dev, void * userdata), void * userdata)
{
    modbus_dev_t * dev = _modbus_get_first_dev();
    while(dev)
    {
        if (exit_cb(dev, userdata))
            return true;
        dev = _modbus_get_next_dev(dev);
    }
    return false;
}


bool           modbus_dev_for_each_reg(modbus_dev_t * dev, bool (exit_cb)(modbus_reg_t * reg, void * userdata), void * userdata)
{
    modbus_reg_t * reg = _modbus_get_first_reg(dev);
    while(reg)
    {
        if (exit_cb(reg, userdata))
            return true;
        reg = _modbus_get_next_reg(reg);
    }
    return false;
}


uint16_t          modbus_reg_get_unit_id(modbus_reg_t * reg)
{
    if (!reg)
        return 0;
    return reg->unit_id;
}



void           modbus_dev_del(modbus_dev_t * dev)
{
    if (!dev)
        return;

    modbus_reg_t * reg = _modbus_get_first_reg(dev);
    while(reg)
    {
        modbus_reg_t * next_reg = _modbus_get_next_reg(reg);

        _modbus_free(reg);

        reg = next_reg;
    }

    modbus_dev_t * mem_dev = _modbus_get_first_dev();
    if (dev == mem_dev)
    {
        modbus_bus->first_dev_offset = dev->next_dev_offset;
        _modbus_free(dev);
        return;
    }

    while (mem_dev->next_dev_offset)
    {
        modbus_dev_t* next_dev = _modbus_get_next_dev(mem_dev);
        if (next_dev == dev)
        {
            mem_dev->next_dev_offset = dev->next_dev_offset;
            _modbus_free(dev);
            return;
        }
        mem_dev = next_dev;
    }
    modbus_debug("Failed to find device!");
}


modbus_reg_t* modbus_get_reg(char * name)
{
    if (!name)
        return NULL;

    modbus_dev_t * dev = _modbus_get_first_dev();
    while(dev)
    {
        modbus_reg_t * reg = modbus_dev_get_reg_by_name(dev, name);
        if (reg)
            return reg;

        dev = _modbus_get_next_dev(dev);
    }
    return NULL;
}


void modbus_reg_del(modbus_reg_t * reg)
{
    if (!reg)
        return;

    modbus_dev_t * dev = modbus_reg_get_dev(reg);

    if (!dev)
        modbus_debug("Failed to find device of register!");

    modbus_reg_t * dev_reg = _modbus_get_first_reg(dev);

    if (dev_reg == reg)
    {
        dev->first_reg_offset = dev_reg->next_reg_offset;
        _modbus_free(reg);
        return;
    }

    while(dev_reg)
    {
        modbus_reg_t * next_reg = _modbus_get_next_reg(dev_reg);

        if (next_reg == reg)
        {
            dev_reg->next_reg_offset = reg->next_reg_offset;
            _modbus_free(reg);
            return;
        }

        dev_reg = next_reg;
    }

    modbus_debug("Failed to find register in device!");
}


modbus_dev_t * modbus_add_device(unsigned unit_id, char *name, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (!name || !unit_id)
        return NULL;

    unsigned len = strlen(name);

    if (len > MODBUS_NAME_LEN)
        return NULL;

    if (modbus_get_device_by_name(name))
        return NULL;

    modbus_dev_t * dev = _modbus_allocate_block();
    if (!dev)
        return NULL;

    memset(dev, 0, sizeof(modbus_dev_t));
    memcpy(dev->name, name, len);
    dev->unit_id = unit_id;
    dev->byte_order = byte_order;
    dev->word_order = word_order;
    dev->next_dev_offset = modbus_bus->first_dev_offset;
    modbus_bus->first_dev_offset = _modbus_get_offset(dev);
    modbus_debug("Added device 0x%"PRIx16" \"%."STR(MODBUS_NAME_LEN)"s\"", unit_id, name);
    return dev;
}


bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr)
{
    if (!dev || !name)
        return false;

    unsigned name_len = strlen(name);

    if (name_len > MODBUS_NAME_LEN)
    {
        modbus_debug("Name too long");
        return false;
    }

    if (func != MODBUS_READ_HOLDING_FUNC && func != MODBUS_READ_INPUT_FUNC)
    {
        modbus_debug("Unsupported func");
        return false;
    }

    if (modbus_dev_get_reg_by_name(dev, name))
        return false;

    modbus_reg_t * dst_reg = _modbus_allocate_block();
    if (!dst_reg)
        return false;

    memset(dst_reg, 0, sizeof(modbus_reg_t));
    memcpy(dst_reg->name, name, name_len);
    dst_reg->reg_addr    = reg_addr;
    dst_reg->type        = type;
    dst_reg->func        = func;
    dst_reg->value_data  = 0;
    dst_reg->value_state = 0;
    dst_reg->unit_id     = dev->unit_id;
    dst_reg->next_reg_offset = dev->first_reg_offset;

    dev->first_reg_offset = _modbus_get_offset(dst_reg);

    return true;
}


bool              modbus_reg_get_name(modbus_reg_t * reg, char name[MODBUS_NAME_LEN + 1])
{
    if (!reg)
        return false;

    memcpy(name, reg->name, MODBUS_NAME_LEN);
    name[MODBUS_NAME_LEN] = 0;
    return true;
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


bool              modbus_reg_get_i16(modbus_reg_t * reg, int16_t * value)
{
    return modbus_reg_get_u16(reg, (uint16_t * )value);
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


bool              modbus_reg_get_i32(modbus_reg_t * reg, int32_t * value)
{
    return modbus_reg_get_u32(reg, (uint32_t * )value);
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
    // cppcheck-suppress invalidPointerCast
    *value = *(float*)&v;
    #pragma GCC diagnostic pop

    return true;
}


modbus_dev_t    * modbus_reg_get_dev(modbus_reg_t * reg)
{
    if (!reg)
        return NULL;

    return modbus_get_device_by_id(reg->unit_id);
}


modbus_reg_state_t modbus_reg_get_state(modbus_reg_t * reg)
{
    if (!reg)
        return MB_REG_INVALID;
    return (modbus_reg_state_t)reg->value_state;
}


void modbus_bus_init(modbus_bus_t * bus)
{
    modbus_bus = bus;

    if (modbus_bus->version == MODBUS_BLOB_VERSION)
    {
        modbus_debug("Loaded modbus defs");
    }
    else
    {
        modbus_debug("Failed to load modbus defs");
        memset(modbus_bus, 0, sizeof(modbus_bus_t));
        modbus_bus->version = MODBUS_BLOB_VERSION;
        modbus_bus->baudrate    = MODBUS_SPEED;
        modbus_bus->databits    = MODBUS_DATABITS;
        modbus_bus->parity      = MODBUS_PARITY;
        modbus_bus->stopbits    = MODBUS_STOP;
        modbus_bus->binary_protocol = false;
        modbus_bus->first_free_offset = _modbus_get_offset(modbus_bus->blocks);
        for(unsigned n = 0; n < (MODBUS_BLOCKS-1) /*Last is zeroed*/; n++)
            modbus_bus->blocks[n].next_free_offset = _modbus_get_offset(&modbus_bus->blocks[n+1]);
    }
}


/* Return true  if different
 *        false if same      */
bool modbus_persist_config_cmp(modbus_bus_t* d0, modbus_bus_t* d1)
{
    /* Is bus different */
    if (d0->version                 != d1->version              ||
        d0->binary_protocol         != d1->binary_protocol      ||
        d0->databits                != d1->databits             ||
        d0->stopbits                != d1->stopbits             ||
        d0->parity                  != d1->parity               ||
        d0->dev_count               != d1->dev_count            ||
        d0->baudrate                != d1->baudrate             ||
        d0->first_dev_offset        != d1->first_dev_offset     ||
        d0->first_free_offset       != d1->first_free_offset    )
    {
        return true;
    }

    modbus_dev_t* d0dev = (modbus_dev_t*)_modbus_get_first_dev2(d0);
    modbus_dev_t* d1dev = (modbus_dev_t*)_modbus_get_first_dev2(d1);
    modbus_reg_t* d0reg;
    modbus_reg_t* d1reg;
    unsigned recursion_count = 0;
    while (recursion_count < MODBUS_BLOCKS)
    {
        if (!d0dev && !d1dev)
        {
            break;
        }
        if ((!d0dev && d1dev) || (d0dev && !d1dev))
        {
            return true;
        }
        /* Only remaining is both d0dev and d1dev exist */
        if (!(d0dev && d1dev && memcmp(d0dev, d1dev, sizeof(modbus_dev_t)) == 0))
        {
            return true;
        }
        d0reg = _modbus_get_first_reg(d0dev);
        d1reg = _modbus_get_first_reg(d1dev);
        while(recursion_count < MODBUS_BLOCKS)
        {
            if ((!d0reg && d1reg) || (d0reg && !d1reg))
            {
                return true;
            }
            if (!d0reg && !d1reg)
            {
                break;
            }
            bool reg_is_same =
                memcmp(d0reg->name, d1reg->name, sizeof(char) * MODBUS_NAME_LEN) == 0 &&
                d0reg->type == d1reg->type &&
                d0reg->func == d1reg->func &&
                d0reg->reg_addr == d1reg->reg_addr &&
                d0reg->unit_id == d1reg->unit_id &&
                d0reg->next_reg_offset == d1reg->next_reg_offset;
            if (!reg_is_same)
            {
                return true;
            }
            d0reg = _modbus_get_next_reg(d0reg);
            d1reg = _modbus_get_next_reg(d1reg);
            recursion_count++;
        }
        d0dev = _modbus_get_next_dev(d0dev);
        d1dev = _modbus_get_next_dev(d1dev);
        recursion_count++;
    }
    return recursion_count >= MODBUS_BLOCKS;
}
