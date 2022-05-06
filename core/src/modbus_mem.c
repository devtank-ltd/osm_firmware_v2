#include <stdlib.h>
#include <string.h>

#include "modbus_mem.h"
#include "log.h"
#include "persist_config.h"


void modbus_log()
{
    modbus_bus_t * bus = persist_get_modbus_bus();

    log_out("Modbus @ %s %"PRIu32" %u%c%s", (bus->binary_protocol)?"BIN":"RTU", bus->baudrate, bus->databits, uart_parity_as_char(bus->parity), uart_stop_bits_as_str(bus->stopbits));

    modbus_dev_t * modbus_devices = bus->modbus_devices;
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = modbus_devices + n;
        if (dev->name[0])
        {
            char byte_char = 'M';
            char word_char = 'M';
            if (dev->byte_order == MODBUS_BYTE_ORDER_LSB)
                byte_char = 'L';
            if (dev->word_order == MODBUS_WORD_ORDER_LSW)
                word_char = 'L';
            log_out("- Device - 0x%"PRIx16" \"%."STR(MODBUS_NAME_LEN)"s\" %cSB %cSW", dev->slave_id, dev->name, byte_char, word_char);
            for (unsigned i = 0; i < MODBUS_DEV_REGS; i++)
            {
                modbus_reg_t * reg = &dev->regs[i];
                if (reg->name[0])
                    log_out("  - Reg - 0x%"PRIx16" (F:%"PRIu8") \"%."STR(MODBUS_NAME_LEN)"s\" %s", reg->reg_addr, reg->func, reg->name, modbus_reg_type_get_str(reg->type));
            }
        }
    }
}

modbus_reg_type_t modbus_reg_type_from_str(const char * type, const char ** pos)
{
    if (!type)
        return MODBUS_REG_TYPE_INVALID;

    if (type[0] == 'U')
    {
        if (type[1] == '1' && type[2] == '6' && (type[3] == ' '|| type[3] == 0))
        {
            if (pos)
                *pos = type+3;
            return MODBUS_REG_TYPE_U16;
        }
        else if (type[1] == '3' && type[2] == '2' && (type[3] == ' '|| type[3] == 0))
        {
            if (pos)
                *pos = type+3;
            return MODBUS_REG_TYPE_U32;
        }
    }
    else if (type[0] == 'F' && (type[1] == ' ' || type[1] == 0))
    {
        if (pos)
            *pos = type+2;
        return MODBUS_REG_TYPE_FLOAT;
    }

    log_out("Unknown modbus reg type.");
    return MODBUS_REG_TYPE_INVALID;
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


static bool _modbus_reg_is_valid(modbus_reg_t * reg)
{
    return reg->class_data_b;
}


static uint32_t _modbus_reg_get(modbus_reg_t * reg)
{
    return reg->class_data_a;
}


static modbus_reg_t * _modbus_dev_get_reg_by_index(modbus_dev_t * dev, unsigned index)
{
    for(unsigned n=0;n<MODBUS_DEV_REGS;n++)
    {
        if (dev->regs[n].name[0])
        {
            if (!index)
                return &dev->regs[n];
            index--;
        }
    }
    return NULL;
}


unsigned      modbus_get_device_count(void)
{
    modbus_dev_t * modbus_devices = persist_get_modbus_bus()->modbus_devices;
    unsigned r = 0;
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
        if (modbus_devices[n].name[0])
            r++;
    return r;
}


modbus_dev_t * modbus_get_device(unsigned index)
{
    modbus_dev_t * modbus_devices = persist_get_modbus_bus()->modbus_devices;
    if (index >= MODBUS_MAX_DEV)
        return NULL;
    return &modbus_devices[index];
}

modbus_dev_t * modbus_get_device_by_id(unsigned slave_id)
{
    modbus_dev_t * modbus_devices = persist_get_modbus_bus()->modbus_devices;
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = &modbus_devices[n];
        if (dev->name[0] && dev->slave_id == slave_id)
            return &modbus_devices[n];
    }
    return NULL;
}


modbus_dev_t * modbus_get_device_by_name(char * name)
{
    if (!name)
        return NULL;
    unsigned name_len = strlen(name);
    if (name_len > MODBUS_NAME_LEN)
        return NULL;
    modbus_dev_t * modbus_devices = persist_get_modbus_bus()->modbus_devices;
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = &modbus_devices[n];
        if (strncmp(name, dev->name, MODBUS_NAME_LEN) == 0)
            return dev;
    }
    return NULL;
}


modbus_reg_t * modbus_dev_get_reg_by_name(modbus_dev_t * dev, char * name)
{
    if (!dev || !name)
        return NULL;
    unsigned name_len = strlen(name);
    if (name_len > MODBUS_NAME_LEN)
        return NULL;
    for(unsigned n = 0; n < MODBUS_DEV_REGS; n++)
    {
        modbus_reg_t * reg = &dev->regs[n];
        if (strncmp(name, reg->name,  MODBUS_NAME_LEN) == 0)
            return reg;
    }
    return NULL;
}


uint16_t       modbus_dev_get_slave_id(modbus_dev_t * dev)
{
    if (!dev)
        return 0;
    return dev->slave_id;
}

void           modbus_dev_del(modbus_dev_t * dev)
{
    if (!dev)
        return;
    memset(dev, 0, sizeof(modbus_dev_t));
}


modbus_reg_t* modbus_get_reg(char * name)
{
    if (!name)
        return NULL;
    for(unsigned n = 0; n < modbus_get_device_count(); n++)
    {
        modbus_reg_t * reg = modbus_dev_get_reg_by_name(modbus_get_device(n), name);
        if (reg)
            return reg;
    }
    return NULL;
}


void modbus_reg_del(modbus_reg_t * reg)
{
    if (!reg)
        return;
    reg->name[0]=0;
}


modbus_dev_t * modbus_add_device(unsigned slave_id, char *name, modbus_byte_orders_t byte_order, modbus_word_orders_t word_order)
{
    if (!name || !slave_id)
        return NULL;

    unsigned len = strlen(name);

    if (len > MODBUS_NAME_LEN)
        return NULL;

    modbus_dev_t * modbus_devices = persist_get_modbus_bus()->modbus_devices;
    for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
    {
        modbus_dev_t * dev = &modbus_devices[n];
        if (!dev->name[0])
        {
            memset(dev->name, 0, MODBUS_NAME_LEN);
            memcpy(dev->name, name, len);
            dev->slave_id = slave_id;
            dev->byte_order = byte_order;
            dev->word_order = word_order;
            modbus_debug("Added device 0x%"PRIx16" \"%."STR(MODBUS_NAME_LEN)"s\"", slave_id, name);
            return dev;
        }
    }

    return NULL;
}


bool           modbus_dev_add_reg(modbus_dev_t * dev, char * name, modbus_reg_type_t type, uint8_t func, uint16_t reg_addr)
{
    if (!dev || !name)
        return false;

    modbus_reg_t * dst_reg = NULL;

    for(unsigned n = 0; n < MODBUS_DEV_REGS; n++)
    {
        if (!dev->regs[n].name[0])
        {
            dst_reg = &dev->regs[n];
            break;
        }
    }

    if (!dst_reg)
    {
        modbus_debug("Dev has max regs.");
        return false;
    }

    if (modbus_get_reg(name) != NULL)
    {
        modbus_debug("Name used.");
        return false;
    }

    if (func != MODBUS_READ_HOLDING_FUNC && func != MODBUS_READ_INPUT_FUNC)
    {
        modbus_debug("Unsupported func");
        return false;
    }

    unsigned name_len = strlen(name);

    if (name_len > MODBUS_NAME_LEN)
    {
        modbus_debug("Name too long");
        return false;
    }
    memset(dst_reg->name, 0, MODBUS_NAME_LEN);
    memcpy(dst_reg->name, name, name_len);
    dst_reg->reg_addr  = reg_addr;
    dst_reg->type      = type;
    dst_reg->func      = func;

    /* For the current types, this start as zero. */
    dst_reg->class_data_a = 0;
    dst_reg->class_data_b = 0;

    return true;
}


unsigned       modbus_dev_get_reg_num(modbus_dev_t * dev)
{
    if (!dev)
        return 0;

    unsigned r=0;
    for (unsigned i = 0; i < MODBUS_DEV_REGS; i++)
    {
        modbus_reg_t * reg = &dev->regs[i];
        if (reg->name[0])
            r++;
    }

    return r;
}


modbus_reg_t * modbus_dev_get_reg(modbus_dev_t * dev, unsigned index)
{
    if (!dev)
        return NULL;
    if (index >= MODBUS_DEV_REGS)
        return NULL;
    return _modbus_dev_get_reg_by_index(dev, index);
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

    modbus_dev_t * modbus_devices = persist_get_modbus_bus()->modbus_devices;
    for(unsigned n=1; n < MODBUS_MAX_DEV; n++)
    {
        if ((uintptr_t)reg < (uintptr_t)&modbus_devices[n] &&
            (uintptr_t)reg > (uintptr_t)&modbus_devices[n-1])
        return &modbus_devices[n-1];
    }

    return NULL;
}


bool              modbus_for_all_regs(modbus_reg_cb cb, void * userdata)
{
    if (!cb)
        return false;

    modbus_bus_t * bus = persist_get_modbus_bus();

    for(unsigned n = 0; n < bus->max_dev_num; n++)
    {
        modbus_dev_t * dev = &bus->modbus_devices[n];

        if (!dev->name[0])
            continue;

        for(unsigned n = 0; n < bus->max_reg_num; n++)
        {
            modbus_reg_t * reg = &dev->regs[n];

            if (!reg->name[0])
                continue;

            if (!cb(reg, userdata))
                return false;
        }
    }

    return true;
}
