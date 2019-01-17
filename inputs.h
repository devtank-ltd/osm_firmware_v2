#ifndef __INPUTS__
#define __INPUTS__

extern void     inputs_init();

extern void     inputs_do_sample();
extern void     inputs_second_boardary();                                    

extern unsigned inputs_get_count();

extern void     inputs_get_input_counts(unsigned input, unsigned * on_count, unsigned * off_count);

extern void     inputs_log();

#endif //__INPUTS__
