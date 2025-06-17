#include <string.h>
#include <stdlib.h>

#include <osm/core/log.h>
#include <osm/core/modbus.h>
#include <osm/core/persist_config.h>
#include <osm/core/persist_config_header.h>
#include <osm/core/update.h>
#include <osm/core/platform.h>
#include <osm/core/common.h>

static uint8_t _fw_page[FLASH_PAGE_SIZE] __attribute__ ( (aligned (16)));

static int _fw_ota_pos =-1;


static bool _fw_ota_flush_page(unsigned fw_page_index)
{
    unsigned abs_page = NEW_FW_PAGE + fw_page_index;
    //uintptr_t dst = NEW_FW_ADDR + (fw_page_index * FLASH_PAGE_SIZE);
    uintptr_t dst = osm_platform_get_fw_addr(fw_page_index);

    osm_fw_debug("Writing page %u (%p) pos:%u", abs_page, (void*)dst, _fw_ota_pos);

    unsigned retries = 3;

    while(retries--)
    {
        osm_fw_debug("%"PRIx64" %"PRIx64, *(uint64_t*)dst, *(uint64_t*)_fw_page);
        if (osm_platform_overwrite_fw_page(dst, abs_page, _fw_page))
        {
            osm_fw_debug("Written page");
            memset(_fw_page, 0, FLASH_PAGE_SIZE);
            return true;
        }

        if (retries)
        {
            osm_log_error("Retrying to write page");
            osm_platform_clear_flash_flags();
        }
    }

    osm_log_error("Failed to write FW page");
    return false;
}


void osm_fw_ota_reset(void)
{
    _fw_ota_pos = -1;
    osm_fw_debug("Reset FW download.");
}


bool osm_fw_ota_add_chunk(void * data, unsigned size, cmd_ctx_t * ctx)
{
    if (_fw_ota_pos < 0)
    {
        osm_fw_debug("Start FW download.");
        _fw_ota_pos = 0;
        memset(_fw_page, 0xFF, FLASH_PAGE_SIZE);
        osm_persist_set_fw_ready(0);
    }

    if (size > FLASH_PAGE_SIZE)
    {
        osm_cmd_ctx_error(ctx,"Firmware chunk too big.");
        _fw_ota_pos = -1;
        return false;
    }

    if ((_fw_ota_pos + size) > FW_MAX_SIZE)
    {
        osm_cmd_ctx_error(ctx,"Firmware update too big (%u > %u)", _fw_ota_pos + size, FW_MAX_SIZE);
        _fw_ota_pos = -1;
        return false;
    }

    unsigned start_page     = _fw_ota_pos / FLASH_PAGE_SIZE;
    unsigned start_page_pos = _fw_ota_pos % FLASH_PAGE_SIZE;

    unsigned next_page_pos = start_page_pos + size;

    _fw_ota_pos += size;

    if (next_page_pos < FLASH_PAGE_SIZE)
    {
        memcpy(_fw_page + start_page_pos, data, size);
    }
    else
    {
        unsigned over_page = next_page_pos % FLASH_PAGE_SIZE;
        unsigned remainer = size - over_page;
        if (remainer)
            memcpy(_fw_page + start_page_pos, data, remainer);
        if (!_fw_ota_flush_page(start_page))
            return false;
        if (over_page)
            memcpy(_fw_page, ((uint8_t*)data) + remainer, over_page);
    }
    return true;
}

bool osm_fw_ota_complete(uint16_t crc)
{
    unsigned cur_page_pos = _fw_ota_pos % FLASH_PAGE_SIZE;
    unsigned pages        = _fw_ota_pos / FLASH_PAGE_SIZE;
    osm_fw_debug("Size:%u", _fw_ota_pos);
    if (cur_page_pos)
    {
        osm_fw_debug("Unwritten:%u", cur_page_pos);
        _fw_ota_flush_page(pages);
    }
    uint16_t data_crc = osm_modbus_crc((uint8_t*)osm_platform_get_fw_addr(0), _fw_ota_pos);
    osm_fw_debug("CRC:0x%04x", data_crc);
    if (data_crc != crc)
    {
        osm_fw_debug("CRC should have been 0x%04x", crc);
        _fw_ota_pos = -1;
        return false;
    }
    osm_platform_finish_fw();
    osm_persist_set_fw_ready(_fw_ota_pos);
    _fw_ota_pos = -1;
    osm_fw_debug("New FW ready.");
    return true;
}


static command_response_t _fw_add(char *args, cmd_ctx_t * ctx)
{
    args = osm_skip_space(args);
    unsigned len = strlen(args);
    if (len%2)
    {
        osm_cmd_ctx_out(ctx,"Invalid fw chunk.");
        return COMMAND_RESP_ERR;
    }
    char * end = args + len;
    while(args < end)
    {
        char * next = args + 2;
        char t = *next;
        *next=0;
        uint8_t d = strtoul(args, NULL, 16);
        *next=t;
        args=next;
        if (!osm_fw_ota_add_chunk(&d, 1, ctx))
        {
            osm_cmd_ctx_out(ctx,"Invalid fw.");
            return COMMAND_RESP_ERR;
        }
    }
    osm_cmd_ctx_out(ctx,"FW %u chunk added", len/2);
    return COMMAND_RESP_OK;
}


static command_response_t _fw_fin(char *args, cmd_ctx_t * ctx)
{
    args = osm_skip_space(args);
    uint16_t crc = strtoul(args, NULL, 16);
    if (osm_fw_ota_complete(crc))
    {
        osm_cmd_ctx_out(ctx,"FW added");
        return COMMAND_RESP_OK;
    }
    osm_cmd_ctx_out(ctx,"FW adding failed.");
    return COMMAND_RESP_ERR;
}


static command_response_t _fw_reset(char *args, cmd_ctx_t * ctx)
{
    osm_persist_commit();
    osm_platform_hard_reset_sys();
    return COMMAND_RESP_OK;
}


struct cmd_link_t* osm_update_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "fw+",          "Add chunk of new fw.",     _fw_add                       , false , NULL },
                                       { "fw@",          "Finishing crc of new fw.", _fw_fin                       , false , NULL },
                                       { "fw_reset",     "Reset into new firmware.", _fw_reset                     , false , NULL }};
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
