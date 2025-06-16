#pragma once

void osm_can_impl_init(void);
void osm_can_impl_send(uint32_t id, uint8_t* data, unsigned len);
void osm_can_impl_send_example(void);
struct cmd_link_t* osm_can_impl_add_commands(struct cmd_link_t* tail);
