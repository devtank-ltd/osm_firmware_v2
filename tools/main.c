#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <json-c/json.h>
#include <json-c/json_util.h>

#include "pinmap.h"
#include "modbus_mem.h"

#include "persist_config_header.h"


static persist_storage_t _config = {.version = PERSIST__VERSION,
                                    .log_debug_mask = DEBUG_SYS};

static void print_help(void)
{
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "filename: (config img filename)\n");
    exit(EXIT_FAILURE);
}


extern void log_debug(uint32_t flag, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("DEBUG: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}


extern void log_out(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}


extern modbus_dev_t* persist_get_modbus_devs(void)
{
    return _config.modbus_devices;
}


extern modbus_bus_t* persist_get_modbus_bus(void)
{
    return &_config.modbus_bus;
}

static char _input_buffer[1024*1024];


static int _read_config_img(const char * filename)
{
    FILE * f = fopen(filename,"r");

    if (!f)
    {
        perror("Failed to open file.");
        return EXIT_FAILURE;
    }
    struct json_object * root = json_object_new_object();
    if (fread(&_config, sizeof(_config), 1, f) != 1)
    {
        perror("Failed to read file.");
        json_object_put(root);
        fclose(f);
        return EXIT_FAILURE;
    }

    json_object_object_add(root, "version", json_object_new_int(PERSIST__VERSION));

    json_object_object_add(root, "log_debug_mask", json_object_new_int(DEBUG_SYS));

    json_object_object_add(root, "mins_interval", json_object_new_int(15));

    modbus_bus_t* bus = &_config.modbus_bus;

    if (bus->version == MODBUS_BLOB_VERSION &&
        bus->max_dev_num == MODBUS_MAX_DEV &&
        bus->max_reg_num == MODBUS_DEV_REGS)
    {
        modbus_log();
    }

    json_object_to_fd(1, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    fclose(f);

    return EXIT_SUCCESS;
}


static int _get_defaulted_int(struct json_object * root, char * name, int default_value) 
{
    struct json_object * tmp = json_object_object_get(root, name);
    if (!tmp)
        return default_value;
    return json_object_get_int(tmp);
}


static int _write_config_img(const char * filename)
{
    struct json_object * root = json_object_from_fd(0);
    if (root)
    {
        perror("Failed to read json.");
        return EXIT_FAILURE;
    }

    struct json_object * obj = json_object_object_get(root, "version");
    if (!obj)
    {
        fprintf(stderr, "No version given.\n");
        json_object_put(root);
        return EXIT_FAILURE;
    }

    memset(&_config, 0, sizeof(_config));

    _config.version = json_object_get_int(obj);
    if (_config.version != PERSIST__VERSION)
    {
        fprintf(stderr, "Wrong version.\n");
        json_object_put(root);
        return EXIT_FAILURE;
    }

    _config.log_debug_mask =  _get_defaulted_int(root, "log_debug_mask", DEBUG_SYS);
    _config.mins_interval =  _get_defaulted_int(root, "mins_interval", 15);

    FILE * f = fopen(filename,"w");

    if (!f)
    {
        json_object_put(root);
        perror("Failed to open file.");
        return EXIT_FAILURE;
    }

    if (fwrite(&_config, sizeof(_config), 1, f) != 1)
    {
        perror("Failed to write file.");
        json_object_put(root);
        fclose(f);
        return EXIT_FAILURE;
    }

    json_object_put(root);
    fclose(f);

    return EXIT_SUCCESS;
}



int main(int argc, char *argv[])
{
    if (argc < 2)
        print_help();

    const char * filename   = argv[1];

    if (isatty(0))
        return _read_config_img(filename);
    else
        return _write_config_img(filename);
}
