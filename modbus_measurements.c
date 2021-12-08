#include "modbus_measurements.h"
#include "modbus.h"

uint32_t modbus_bus_collection_time(void)
{
    return 500;
}

bool modbus_init(char* name)
{
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return false;
    return modbus_start_read(reg);
}


bool modbus_get(char* name, value_t* value)
{
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return false;
    switch(modbus_reg_get_type(reg))
    {
        case MODBUS_REG_TYPE_U16:
        case MODBUS_REG_TYPE_U32:
        case MODBUS_REG_TYPE_FLOAT:
        default: break;
    }
   return false;
}

