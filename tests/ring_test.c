#include <stdio.h>
#include <string.h>

#include "ring.h"

#define NORMAL   "\033[39m"
#define OKGREEN  "\033[92m"
#define BADRED   "\033[91m"

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

static char * test_lines[] = {"The rain in Spain stays\n",
                              "on the plane. Yes, plane.\n",
                              "It's a hot country.\n"};


static void basic_test(char * test, unsigned expected, unsigned got)
{
    if (expected == got)
        printf(OKGREEN"%s %u == %u - pass"NORMAL"\n", test, expected, got);
    else
        printf(BADRED"%s %u != %u FAIL"NORMAL"\n", test, expected, got);
}


int main(int argc, char * argv)
{
    char buf[512];

    ring_buf_t ring = RING_BUF_INIT(buf, sizeof(buf));

    basic_test("Init", 0, ring_buf_get_pending(&ring));

    for(unsigned n = 0; n < ARRAY_SIZE(test_lines); n++)
    {
        char * line = test_lines[n];

        unsigned before = ring_buf_get_pending(&ring);
        ring_buf_add_str(&ring, line);
        basic_test("Added", before + strlen(line), ring_buf_get_pending(&ring));
    }

    for(unsigned n = 0; n < ARRAY_SIZE(test_lines); n++)
    {
        char * org_line = test_lines[n];
        unsigned org_line_len = strlen(org_line) - 1;

        char line[128];

        unsigned before = ring_buf_get_pending(&ring);

        ring_buf_readline(&ring, line, sizeof(line));

        basic_test("Read line", org_line_len, strlen(line));
        basic_test("Removed", before - org_line_len - 1, ring_buf_get_pending(&ring));
        basic_test("Data Diff", 0, strncmp(org_line, line, org_line_len));
    }

    for(unsigned n = 0; n < ARRAY_SIZE(test_lines); n++)
    {
        char * line = test_lines[n];

        unsigned before = ring_buf_get_pending(&ring);
        ring_buf_add_str(&ring, line);
        basic_test("Added", before + strlen(line), ring_buf_get_pending(&ring));
    }

    for(unsigned n = 0; n < ARRAY_SIZE(test_lines); n++)
    {
        char * org_line = test_lines[n];
        unsigned org_line_len = strlen(org_line);

        char line[128] = {0};
        unsigned total = 0;

        while(total < org_line_len)
        {
            unsigned chunk = org_line_len - total;
            if (chunk > 8)
                chunk = 8;
            unsigned got = ring_buf_read(&ring, line + total, chunk);
            basic_test("Chunk", chunk, got);
            total += got;
        }
        basic_test("Total", org_line_len, total);
        line[total] = 0;
        basic_test("Data Diff", 0, strncmp(org_line, line, org_line_len));
    }
    
    return 0;
}
