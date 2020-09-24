#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "basic_fixed.h"


int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        printf("Requires <num> (<op> <num> .....)\n");
        return -1;
    }

    char buffer[32];

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

        if (getenv("DEBUG"))
        {
            if (basic_fixed_to_str(&a, buffer, sizeof(buffer)) < 0)
            {
                fprintf(stderr, "Bugger printing A\n");
                return -1;
            }

            fprintf(stderr, "A:%s\n", buffer);
            fprintf(stderr, "op:%s\n", argv[n]);
            if (basic_fixed_to_str(&b, buffer, sizeof(buffer)) < 0)
            {
                fprintf(stderr, "Bugger printing B\n");
                return -1;
            }

            fprintf(stderr, "B:%s\n", buffer);
            if (basic_fixed_to_str(&v, buffer, sizeof(buffer)) < 0)
            {
                fprintf(stderr, "Bugger printing B\n");
                return -1;
            }
            fprintf(stderr, "Result:%s\n", buffer);
        }
    }

    if (basic_fixed_to_str(&v, buffer, sizeof(buffer)) < 0)
    {
        fprintf(stderr, "Bugger result\n");
        return -1;
    }

    printf("%s\n", buffer);

    return 0;
}
