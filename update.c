#include <string.h>

#include <libopencm3/stm32/flash.h>

#include "log.h"
#include "modbus.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "update.h"

static uint8_t page[FLASH_PAGE_SIZE];

static int fw_ota_pos =-1;


static void _fw_ota_flush_page(unsigned cur_page)
{
    uintptr_t dst = FLASH_PAGE_SIZE * cur_page;
    flash_unlock();
    flash_erase_page(cur_page);
    flash_program(dst, page, FLASH_PAGE_SIZE);
    flash_lock();
    memset(page, 0xFF, FLASH_PAGE_SIZE);
}



bool fw_ota_add_chunk(void * data, unsigned size)
{
    if (fw_ota_pos < 0)
    {
        fw_ota_pos = 0;
        memset(page, 0xFF, FLASH_PAGE_SIZE);
        persist_set_new_fw_ready(false);
    }

    if ((fw_ota_pos + size) > FW_MAX_SIZE)
    {
        log_error("Firmware update too big.");
	fw_ota_pos = -1;
	return false;
    }

    unsigned start_page     = NEW_FW_PAGE + (fw_ota_pos / FLASH_PAGE_SIZE);
    unsigned start_page_pos = size + (fw_ota_pos % FLASH_PAGE_SIZE);

    int over_page = start_page_pos - FLASH_PAGE_SIZE;

    if (over_page <= 0)
    {
        memcpy(page + start_page_pos, data, size);
    }
    else
    {
        unsigned remainer = size - over_page;
        memcpy(page + start_page_pos, data, remainer);
        _fw_ota_flush_page(start_page);
        memcpy(page, ((uint8_t*)data) + remainer, over_page);
    }
    return true;
}

bool fw_ota_complete(uint16_t crc)
{
    unsigned cur_page_pos = fw_ota_pos % FLASH_PAGE_SIZE;
    if (cur_page_pos)
        _fw_ota_flush_page(fw_ota_pos / FLASH_PAGE_SIZE);
    uint16_t data_crc = modbus_crc((uint8_t*)NEW_FW_ADDR, fw_ota_pos);
    fw_ota_pos = -1;
    if (data_crc != crc)
        return false;
    persist_set_new_fw_ready(true);
    return true;
}
