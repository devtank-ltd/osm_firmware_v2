#pragma once

void osm_fw_ota_reset(void);

bool osm_fw_ota_add_chunk(void * data, unsigned size, cmd_ctx_t * ctx);

bool osm_fw_ota_complete(uint16_t crc);
struct cmd_link_t* osm_update_add_commands(struct cmd_link_t* tail);
