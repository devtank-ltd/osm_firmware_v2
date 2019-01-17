#ifndef __OUTPUTS__
#define __OUTPUTS__

extern void     outputs_init();

extern void     outputs_set(unsigned output, bool on_off); 

extern unsigned outputs_get_count();

extern void     output_output_log(unsigned output);
extern void     outputs_log();

#endif //__OUTPUTS__
