#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "value.h"

#include "test.h"


#define BUF_SIZE 256


static bool do_test(char * num_a, char op, char * num_b, char buffer[BUF_SIZE])
{
    value_t a, b, r;

    if (!value_from_str(&a, num_a))
    {
        fprintf(stderr, "Failed to parse %s\n", num_a);
        return false;
    }

    if (!value_from_str(&b, num_b))
    {
        fprintf(stderr, "Failed to parse %s\n", num_b);
        return false;
    }

    switch(op)
    {
        case '-': value_sub(&r, &a, &b); break;
        case '/': value_div(&r, &a, &b); break;
        case '+': value_add(&r, &a, &b); break;
        case '*': value_mult(&r, &a, &b); break;
        default:
            fprintf(stderr, "Unknown op type : %c\n", op);
            return false;
    }

    if (!value_to_str(&a, buffer, BUF_SIZE))
    {
        fprintf(stderr, "Failed to turn result to string.\n");

        return false;
    }

    printf("%s ", buffer);

    if (!value_to_str(&b, buffer,BUF_SIZE))
    {
        fprintf(stderr, "Failed to turn result to string.\n");
        return false;
    }

    printf("%c %s", op, buffer);

    if (!value_to_str(&r, buffer, BUF_SIZE))
    {
        fprintf(stderr, "Failed to turn result to string.\n");
        return false;
    }

    printf(" = %s\n", buffer);
    return true;
}



int main(int argc, char * argv[])
{
    char buffer[BUF_SIZE];

    if (argc < 4)
    {
        fprintf(stderr, "\
Requires: <num> <op> <num>\n\
<num> is format:\n\
U8:<int>, I8:<int>,\n\
U16:<int>, I16:<int>,\n\
U32:<int>, I32:<int>,\n\
U64:<int>, I64:<int>,\n\
F32:<float>, F64:<float>\n\n");

        struct test_t
        {
            char* a;
            char op;
            char* b;
            char* r;
        };

        struct test_t tests[] =
        {
            { "U8:1", '+', "U8:1", "U8:2" },
            { "U8:1", '*', "U8:1", "U8:1" },
            { "U8:1", '/', "U8:1", "U8:1" },
            { "U8:1", '-', "U8:1", "U8:0" },
            { "U16:1", '+', "U16:1", "U8:2" },
            { "U16:1", '*', "U16:1", "U8:1" },
            { "U16:1", '/', "U16:1", "U8:1" },
            { "U16:1", '-', "U16:1", "U8:0" },
            { "U32:1", '+', "U32:1", "U8:2" },
            { "U32:1", '*', "U32:1", "U8:1" },
            { "U32:1", '/', "U32:1", "U8:1" },
            { "U32:1", '-', "U32:1", "U8:0" },
            { "U64:1", '+', "U64:1", "U8:2" },
            { "U64:1", '*', "U64:1", "U8:1" },
            { "U64:1", '/', "U64:1", "U8:1" },
            { "U64:1", '-', "U64:1", "U8:0" },

            { "I8:-1", '+', "I8:-1", "I8:-2" },
            { "I8:-1", '*', "I8:-1", "U8:1" },
            { "I8:-1", '/', "I8:-1", "U8:1" },
            { "I8:-1", '-', "I8:-1", "U8:0" },
            { "I16:-1", '+', "I16:-1", "I8:-2" },
            { "I16:-1", '*', "I16:-1", "U8:1" },
            { "I16:-1", '/', "I16:-1", "U8:1" },
            { "I16:-1", '-', "I16:-1", "U8:0" },
            { "I32:-1", '+', "I32:-1", "I8:-2" },
            { "I32:-1", '*', "I32:-1", "U8:1" },
            { "I32:-1", '/', "I32:-1", "U8:1" },
            { "I32:-1", '-', "I32:-1", "U8:0" },
            { "I64:-1", '+', "I64:-1", "I8:-2" },
            { "I64:-1", '*', "I64:-1", "U8:1" },
            { "I64:-1", '/', "I64:-1", "U8:1" },
            { "I64:-1", '-', "I64:-1", "U8:0" },

            { "F32:1.0", '+', "U8:1", "F32:2.000000" },
            { "F32:1.0", '*', "U8:1", "F32:1.000000" },
            { "F32:1.0", '/', "U8:2", "F32:0.500000" },
            { "F32:1.0", '-', "U8:1", "F32:0.000000" },

            { "U8:1",      '+', "U8:255", "U16:256" },
            { "U8:128",    '*', "U8:3",   "U16:384" },
            { "U16:65535", '+', "U8:1",   "U32:65536" },

            { "I8:-128",        '-', "U8:1",  "I16:-129" },
            { "I16:-32768",     '-', "U8:1",  "I32:-32769" },
            { "I32:-2147483648",'-', "U8:1",  "I64:-2147483649" },

            { "U32:4294967295", '+', "U8:1",  "U64:4294967296" },
        };

        for(unsigned n = 0; n < ARRAY_SIZE(tests); n++)
        {
            struct test_t * test = &tests[n];
            if (do_test(test->a, test->op, test->b, buffer) && !strcmp(buffer, test->r))
                printf(OKGREEN"%s %c %s = %s == %s -- pass"NORMAL"\n", test->a, test->op, test->b, test->r, buffer);
            else
            {
                printf(BADRED"%s %c %s = %s != %s -- FAIL"NORMAL"\n", test->a, test->op, test->b, test->r, buffer);
                exit(EXIT_FAILURE);
            }
        }

        return EXIT_SUCCESS;
    }

    if (do_test(argv[1], argv[2][0], argv[3], buffer))
        return EXIT_SUCCESS;
    else
        return EXIT_FAILURE;
}
