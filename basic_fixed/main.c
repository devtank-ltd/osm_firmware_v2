#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "basic_fixed.h"


static void basic_fixed_print(basic_fixed_t * v, const char * prefix)
{
    char buffer[32];
    
    if (basic_fixed_to_str(v, buffer, sizeof(buffer)) < 0)
    {
        fprintf(stderr, "Bugger result\n");
        return;
    }

    printf("%s%s\n", prefix, buffer);
}


int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        printf("Requires <num> (<op> <num> .....)\n");
        return -1;
    }

    basic_fixed_t v;

    if (!basic_fixed_read(argv[1], &v, NULL))
    {
        fprintf(stderr, "Bugger reading\n");
        return -1;
    }

    for(unsigned n = 2; n + 1 < argc; n+=2)
    {
        basic_fixed_t a = v, b;

        if (!basic_fixed_read(argv[n+1], &b, NULL))
        {
            fprintf(stderr, "Bugger reading Next\n");
            return -1;
        }

        basic_fixed_print(&a, "A:");
        printf("op:%s\n", argv[n]);
        basic_fixed_print(&b, "B:");

        switch(argv[n][0])
        {
            case '+' : basic_fixed_add(&v, &a, &b); break;
            case '-' : basic_fixed_sub(&v, &a, &b); break;
            case '*' : basic_fixed_mul(&v, &a, &b); break;
            case '/' : basic_fixed_div(&v, &a, &b); break;
            default:
                fprintf(stderr, "Bugger opp\n");
                return -1;
        }
    }

    basic_fixed_print(&v, "Result:");

    return 0;
}
