#pragma once

extern void fw_ota_reset(void);

extern bool fw_ota_add_chunk(void * data, unsigned size);

extern bool fw_ota_complete(uint16_t crc);
