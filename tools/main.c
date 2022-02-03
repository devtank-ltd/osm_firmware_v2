#include "json_x_img.h"


persist_storage_t osm_config __attribute__((aligned (16))) =
{
    .version = PERSIST__VERSION,
    .log_debug_mask = DEBUG_SYS
};


void log_debug(uint32_t flag, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("DEBUG: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}


void log_out(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}


void log_error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr,"\n");
    va_end(ap);
}


modbus_bus_t* persist_get_modbus_bus(void)
{
    return &osm_config.modbus_bus;
}


static void _print_help(void)
{
    fprintf(stderr, "Arguments:\n"
    "json_x_img <config_img_filename>\n"
    "If STDIN has data, it's read as JSON and config is written out.\n"
    "If STDIN has no data, it's config image is read and JSON is written out to STDOUT.\n"
    "Examples:\n"
    "Read JSON\n"
    "./json_x_img config.img < config.json\n"
    "Write JSON\n"
    "./json_x_img config.img > config.json\n");
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
    if (argc < 2)
        _print_help();

    const char * filename   = argv[1];

    if (isatty(0))
        return write_json_from_img(filename);
    else
        return read_json_to_img(filename);
}
