#include <stdio.h>
#include <string.h>

#include "ring.h"


int main(int argc, char * argv)
{
    char buf[512];

    ring_buf_t ring = RING_BUF_INIT(buf, sizeof(buf));

    printf("Pending %u\n", ring_buf_get_pending(&ring));
    ring_buf_add_str(&ring, "000123456789ABCDFG\n");

    printf("Pending %u\n", ring_buf_get_pending(&ring));
    ring_buf_add_str(&ring, "010123456789ABCDFG\n");

    char line[128];

    printf("Pending %u\n", ring_buf_get_pending(&ring));

    ring_buf_readline(&ring, line, sizeof(line));
    
    printf("Read line \"%s\"\n", line);

    printf("ring %u %u\n", ring.r_pos, ring.w_pos);
    printf("Pending %u\n", ring_buf_get_pending(&ring));

    ring_buf_readline(&ring, line, sizeof(line));
    
    printf("Read line \"%s\"\n", line);

    printf("ring %u %u\n", ring.r_pos, ring.w_pos);
    printf("Pending %u\n", ring_buf_get_pending(&ring));


    

    return 0;
}
