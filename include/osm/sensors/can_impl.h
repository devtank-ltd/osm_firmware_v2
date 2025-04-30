#pragma once

extern void can_impl_init(void);
extern void can_impl_send(uint32_t id, uint8_t* data, unsigned len);
extern void can_impl_send_example(void);
extern struct cmd_link_t* can_impl_add_commands(struct cmd_link_t* tail);
