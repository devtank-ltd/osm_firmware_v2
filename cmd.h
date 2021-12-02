#ifndef __CMDS__
#define __CMDS__

extern void cmds_process(char * command, unsigned len);

extern void cmds_init();

extern char * skip_space(char * pos);

#endif //__CMDS__
