#ifndef __CMDS__
#define __CMDS__

extern bool cmds_add_char(char c);

extern void cmds_process(char * command, unsigned len);

extern void cmds_init();

#endif //__CMDS__
