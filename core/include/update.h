#pragma once

extern void fw_ota_reset(void);

extern bool fw_ota_add_chunk(void * data, unsigned size, cmd_ctx_t * ctx);

extern bool fw_ota_complete(uint16_t crc);
extern struct cmd_link_t* update_add_commands(struct cmd_link_t* tail);
