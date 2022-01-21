#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "persist_config_header.h"


static void print_help(void)
{
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "filename: (output filename)\n");
    fprintf(stderr, "fw_size: (Pending firmware size)\n");
    fprintf(stderr, "debug_mask: (0x Hex of debug flags)\n");
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
    persist_storage_t config = {.version = PERSIST__VERSION,
                                .log_debug_mask = DEBUG_SYS};

    if (argc < 4)
        print_help();

    const char * filename   = argv[1];
    const char * fw_size    = argv[2];
    const char * debug_mask = argv[3];

    if (debug_mask[0] != '0' || tolower(debug_mask[1]) != 'x')
        print_help();

    debug_mask += 2;

    FILE * f = fopen(filename,"w");

    if (!f)
    {
        perror("Failed to open file.");
        return EXIT_FAILURE;
    }

    config.pending_fw = strtoul(fw_size, NULL, 10);
    config.log_debug_mask |= strtoul(debug_mask, NULL, 16);

    if (fwrite(&config, sizeof(config), 1, f) != 1)
    {
        perror("Failed to write file.");
        fclose(f);
        return EXIT_FAILURE;
    }

    fclose(f);

    return EXIT_SUCCESS;
}
