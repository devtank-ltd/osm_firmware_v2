#include <stdio.h>
#include <stdlib.h>

#include "persist_config_header.h"


int main(int argc, char *argv[])
{
    persist_storage_t config = {.version = PERSIST__VERSION,
                                .log_debug_mask = DEBUG_SYS};

    if (argc < 3)
    {
        fprintf(stderr, "Give filename to write and any pending firmware size.\n");
    return EXIT_FAILURE;
    }

    FILE * f = fopen(argv[1],"w");

    if (!f)
    {
        perror("Failed to open file.");
        return EXIT_FAILURE;
    }

    config.pending_fw = strtoul(argv[2], NULL, 10);

    if (fwrite(&config, sizeof(config), 1, f) != 1)
    {
        perror("Failed to write file.");
        fclose(f);
        return EXIT_FAILURE;
    }

    fclose(f);

    return EXIT_SUCCESS;
}
