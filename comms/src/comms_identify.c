#include <stdint.h>
#include <string.h>

#include "base_types.h"
#include "comms_identify.h"
#include "modbus.h"
#include "i2c.h"
#include "common.h"

#define COMMS_IDENTIFY_MEM_VERSION              1

#define COMMS_IDENTIFY_ID_STR_LW                "LW"
#define COMMS_IDENTIFY_ID_STR_WIFI              "WIFI"
#define COMMS_IDENTIFY_ID_STR_POE               "POE"
#define COMMS_IDENTIFY_ID_STR_UNKNOWN           "UNKNOWN"


static comms_type_t _comms_identify_type = COMMS_TYPE_UNKNOWN;


typedef struct
{
    uint8_t mem_version;
    uint8_t comms_type; /* comms_type_t */
    uint16_t crc;
} __attribute__((__packed__)) comms_identify_mem_t;


comms_type_t comms_identify(void)
{
    i2cs_init();
    comms_identify_mem_t mem = {0};
    if (!comms_eeprom_read(&mem, sizeof(comms_identify_mem_t)))
    {
        log_error("COMMS EEPROM: Failed to read");
        return COMMS_TYPE_UNKNOWN;
    }
    if (COMMS_IDENTIFY_MEM_VERSION != mem.mem_version)
    {
        log_error("COMMS EEPROM: Unknown version (%"PRIu8" != %"PRIu8")", mem.mem_version, COMMS_IDENTIFY_MEM_VERSION);
        return COMMS_TYPE_UNKNOWN;
    }
    uint16_t crc = modbus_crc((uint8_t*)&mem, 2);
    if (crc != mem.crc)
    {
        log_error("COMMS EEPROM: Bad CRC (0x%04"PRIX16" != 0x%04"PRIX16")", mem.crc, crc);
        return COMMS_TYPE_UNKNOWN;
    }
    if (!mem.comms_type || COMMS_TYPE_COUNT <= mem.comms_type)
    {
        log_error("COMMS EEPROM: Unknown type (%"PRIu8")", mem.comms_type);
        return COMMS_TYPE_UNKNOWN;
    }
    _comms_identify_type = (comms_type_t)mem.comms_type;
    return _comms_identify_type;
}


static bool _comms_identify_write(void)
{
    comms_identify_mem_t mem;
    mem.mem_version = COMMS_IDENTIFY_MEM_VERSION;
    mem.comms_type = _comms_identify_type;
    mem.crc = modbus_crc((uint8_t*)&mem, 2);
    return comms_eeprom_write(&mem, sizeof(mem));
}


static command_response_t _comms_ident_set_cb(char* args, cmd_ctx_t * ctx)
{
    char* p = skip_space(args);
    unsigned len = strlen(p);
    if (is_str(COMMS_IDENTIFY_ID_STR_LW, p, len))
    {
        cmd_ctx_out(ctx, "Set to: "COMMS_IDENTIFY_ID_STR_LW);
        _comms_identify_type = COMMS_TYPE_LW;
    }
    else if (is_str(COMMS_IDENTIFY_ID_STR_WIFI, p, len))
    {
        cmd_ctx_out(ctx, "Set to: "COMMS_IDENTIFY_ID_STR_WIFI);
        _comms_identify_type = COMMS_TYPE_WIFI;
    }
    else if (is_str(COMMS_IDENTIFY_ID_STR_POE, p, len))
    {
        cmd_ctx_out(ctx, "Set to: "COMMS_IDENTIFY_ID_STR_POE);
        _comms_identify_type = COMMS_TYPE_POE;
    }
    else
    {
        cmd_ctx_out(ctx, "Unknown type '%s' (%u)", p, len);
        return COMMAND_RESP_ERR;
    }
    return COMMAND_RESP_OK;
}


static command_response_t _comms_ident_cb(char* args, cmd_ctx_t * ctx)
{
    comms_type_t type = comms_identify();
    switch (type)
    {
        case COMMS_TYPE_LW:
            cmd_ctx_out(ctx,COMMS_IDENTIFY_ID_STR_LW);
            break;
        case COMMS_TYPE_WIFI:
            cmd_ctx_out(ctx,COMMS_IDENTIFY_ID_STR_WIFI);
            break;
        case COMMS_TYPE_POE:
            cmd_ctx_out(ctx,COMMS_IDENTIFY_ID_STR_POE);
            break;
        case COMMS_TYPE_UNKNOWN:
            /* fall through */
        default:
            cmd_ctx_out(ctx,COMMS_IDENTIFY_ID_STR_UNKNOWN);
            return COMMAND_RESP_ERR;
    }
    return COMMAND_RESP_OK;
}


static command_response_t _comms_ident_write_cb(char* args, cmd_ctx_t * ctx)
{
    if (!_comms_identify_write())
    {
        cmd_ctx_error(ctx,"Failed to write identification");
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx,"Wrote identification");
    return COMMAND_RESP_OK;
}


struct cmd_link_t* comms_identify_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_ident_set"     , "Set COMMS type"      , _comms_ident_set_cb   , true  , NULL },
        { "comms_ident"         , "Intentify COMMS"     , _comms_ident_cb       , false , NULL },
        { "comms_ident_write"   , "Set COMMS EEPROM"    , _comms_ident_write_cb , true  , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
