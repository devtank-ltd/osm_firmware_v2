#pragma once

#include <stdbool.h>


bool linux_add_pty(char* name, uint32_t* fd, void (*read_cb)(char *, unsigned int));
