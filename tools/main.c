#include <stdio.h>
#include <stdlib.h>

#include "persist_config_header.h"


int main(int argc, char *argv[])
{
    persist_storage_t config = {PERSIST__VERSION, DEBUG_SYS, DEFAULT_FW_ADDR};

    if (argc < 2)
    {
        fprintf(stderr, "Give filename to write.\n");
    return EXIT_FAILURE;
    }

    FILE * f = fopen(argv[1],"w");

    if (!f)
    {
        perror("Failed to open file.");
    return EXIT_FAILURE;
    }

    if (fwrite(&config, sizeof(config), 1, f) != 1)
    {
        perror("Failed to write file.");
    fclose(f);
    return EXIT_FAILURE;
    }

    fclose(f);

    return EXIT_SUCCESS;
}
