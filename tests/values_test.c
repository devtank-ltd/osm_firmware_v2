#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "value.h"


int main(int argc, char * argv[])
{
    value_t a, b, r;
    char buffer[256];

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
        return EXIT_FAILURE;
    }

    if (!value_from_str(&a, argv[1]))
    {
        fprintf(stderr, "Failed to parse %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (!value_from_str(&b, argv[3]))
    {
        fprintf(stderr, "Failed to parse %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    char op = argv[2][0];

    switch(op)
    {
        case '-': value_sub(&r, &a, &b); break;
        case '/': value_div(&r, &a, &b); break;
        case '+': value_add(&r, &a, &b); break;
        case '*': value_mult(&r, &a, &b); break;
        default:
            fprintf(stderr, "Unknown op type : %c\n", op);
            return EXIT_FAILURE;
    }

    if (!value_to_str(&a, buffer, sizeof(buffer)))
    {
        fprintf(stderr, "Failed to turn result to string.\n");
        return EXIT_FAILURE;
    }

    printf("%s ", buffer);

    if (!value_to_str(&b, buffer, sizeof(buffer)))
    {
        fprintf(stderr, "Failed to turn result to string.\n");
        return EXIT_FAILURE;
    }

    printf("%c %s", op, buffer);

    if (!value_to_str(&r, buffer, sizeof(buffer)))
    {
        fprintf(stderr, "Failed to turn result to string.\n");
        return EXIT_FAILURE;
    }

    printf(" = %s\n", buffer);

    return EXIT_SUCCESS;
}




