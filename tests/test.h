#pragma once

#define NORMAL   "\033[39m"
#define OKGREEN  "\033[92m"
#define BADRED   "\033[91m"

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

static void basic_test(char * test, unsigned expected, unsigned got)
{
    if (expected == got)
        printf(OKGREEN"%s %u == %u - pass"NORMAL"\n", test, expected, got);
    else
        printf(BADRED"%s %u != %u FAIL"NORMAL"\n", test, expected, got);
}
