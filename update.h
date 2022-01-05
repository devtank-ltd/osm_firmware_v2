#pragma once

#define fw_debug(...) log_debug(DEBUG_FW, "FW: "__VA_ARGS__)

extern bool fw_ota_add_chunk(void * data, unsigned size);

extern bool fw_ota_complete(uint16_t crc);
