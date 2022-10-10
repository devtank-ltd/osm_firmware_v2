#pragma once

#include <stdbool.h>


#define VERSION_NAME_LEN            10


typedef enum
{
    VERSION_ARCH_UNDEFINED,
    VERSION_ARCH_REV_B,
    VERSION_ARCH_REV_C,
} version_arch_t;


version_arch_t  version_get_arch(void);
bool            version_is_arch(version_arch_t is_arch);
void            version_arch_error_handler(void);
