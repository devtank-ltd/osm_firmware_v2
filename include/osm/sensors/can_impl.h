#pragma once

void can_impl_init(void);
void can_impl_send(uint32_t id, uint8_t* data, unsigned len);
void can_impl_send_example(void);
struct cmd_link_t* can_impl_add_commands(struct cmd_link_t* tail);
