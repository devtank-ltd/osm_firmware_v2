#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "modbus.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "update.h"
#include "platform.h"
#include "common.h"

static uint8_t _fw_page[FLASH_PAGE_SIZE] __attribute__ ( (aligned (16)));

static int _fw_ota_pos =-1;


static bool _fw_ota_flush_page(unsigned fw_page_index)
{
    unsigned abs_page = NEW_FW_PAGE + fw_page_index;
    uintptr_t dst = NEW_FW_ADDR + (fw_page_index * FLASH_PAGE_SIZE);

    fw_debug("Writing page %u (%p) pos:%u", abs_page, (void*)dst, _fw_ota_pos);

    unsigned retries = 3;

    while(retries--)
    {
        fw_debug("%"PRIx64" %"PRIx64, *(uint64_t*)dst, *(uint64_t*)_fw_page);
        if (platform_overwrite_fw_page(dst, abs_page, _fw_page))
        {
            fw_debug("Written page");
            return true;
        }

        if (retries)
        {
            log_error("Retrying to write page");
            platform_clear_flash_flags();
        }
    }

    log_error("Failed to write FW page");
    return false;
}


void fw_ota_reset(void)
{
    _fw_ota_pos = -1;
    fw_debug("Reset FW download.");
}


bool fw_ota_add_chunk(void * data, unsigned size)
{
    if (_fw_ota_pos < 0)
    {
        fw_debug("Start FW download.");
        _fw_ota_pos = 0;
        memset(_fw_page, 0xFF, FLASH_PAGE_SIZE);
        persist_set_fw_ready(0);
    }

    if (size > FLASH_PAGE_SIZE)
    {
        log_error("Firmware chunk too big.");
        _fw_ota_pos = -1;
        return false;
    }

    if ((_fw_ota_pos + size) > FW_MAX_SIZE)
    {
        log_error("Firmware update too big.");
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

bool fw_ota_complete(uint16_t crc)
{
    unsigned cur_page_pos = _fw_ota_pos % FLASH_PAGE_SIZE;
    unsigned pages        = _fw_ota_pos / FLASH_PAGE_SIZE;
    fw_debug("Size:%u", _fw_ota_pos);
    if (cur_page_pos)
    {
        fw_debug("Unwritten:%u", cur_page_pos);
        _fw_ota_flush_page(pages);
    }
    uint16_t data_crc = modbus_crc((uint8_t*)NEW_FW_ADDR, _fw_ota_pos);
    fw_debug("CRC:0x%04x", data_crc);
    if (data_crc != crc)
    {
        _fw_ota_pos = -1;
        return false;
    }
    persist_set_fw_ready(_fw_ota_pos);
    _fw_ota_pos = -1;
    fw_debug("New FW ready.");
    return true;
}


static command_response_t _fw_add(char *args)
{
    args = skip_space(args);
    unsigned len = strlen(args);
    if (len%2)
    {
        log_error("Invalid fw chunk.");
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
        if (!fw_ota_add_chunk(&d, 1))
        {
            log_error("Invalid fw.");
            return COMMAND_RESP_ERR;
        }
    }
    log_out("FW %u chunk added", len/2);
    return COMMAND_RESP_OK;
}


static command_response_t _fw_fin(char *args)
{
    args = skip_space(args);
    uint16_t crc = strtoul(args, NULL, 16);
    if (fw_ota_complete(crc))
    {
        log_out("FW added");
        return COMMAND_RESP_OK;
    }
    log_error("FW adding failed.");
    return COMMAND_RESP_ERR;
}


struct cmd_link_t* update_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "fw+",          "Add chunk of new fw.",     _fw_add                       , false , NULL },
                                       { "fw@",          "Finishing crc of new fw.", _fw_fin                       , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
