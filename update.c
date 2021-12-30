#include <string.h>

#include <libopencm3/stm32/flash.h>

#include "log.h"
#include "modbus.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "update.h"

static uint8_t _fw_page[FLASH_PAGE_SIZE] __attribute__ ( (aligned (16)));

static int _fw_ota_pos =-1;


static void _fw_ota_flush_page(unsigned fw_page_index)
{
    unsigned abs_page = NEW_FW_PAGE + fw_page_index;
    uintptr_t dst = NEW_FW_ADDR + (fw_page_index * FLASH_PAGE_SIZE);
    fw_debug("Writing page %u (%p)", abs_page, (void*)dst);
    flash_unlock();
    flash_erase_page(abs_page);
    flash_program(dst, _fw_page, FLASH_PAGE_SIZE);
    flash_lock();
    memset(_fw_page, 0xFF, FLASH_PAGE_SIZE);
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
        fw_debug("pos:%u", _fw_ota_pos);
        unsigned over_page = next_page_pos % FLASH_PAGE_SIZE;
        unsigned remainer = size - over_page;
	if (remainer)
            memcpy(_fw_page + start_page_pos, data, remainer);
        _fw_ota_flush_page(start_page);
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
        _fw_ota_flush_page(pages);
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
