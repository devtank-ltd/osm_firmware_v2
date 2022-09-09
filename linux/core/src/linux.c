#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>


#include "platform.h"
#include "linux.h"


#define LINUX_FILE_LOC          "/tmp/osm/"
#define LINUX_PTY_BUF_SIZ       64
#define LINUX_MAX_PTY           32
#define LINUX_PTY_NAME_SIZE     16

#define LINUX_MASTER_SUFFIX     "_master"
#define LINUX_SLAVE_SUFFIX      "_slave"

#define LINUX_PERSIST_FILE_LOC  "/tmp/osm.img"


extern int errno;


typedef enum
{
    LINUX_FD_TYPE_PTY,
    LINUX_FD_TYPE_IO,
} linux_fd_type_t;


typedef struct
{
    linux_fd_type_t type;
    union
    {
        struct
        {
            int32_t master_fd;
            int32_t slave_fd;
        } pty;
        struct
        {
            int32_t fd;
            FILE*   enabled_file;
            FILE*   direction_file;
            FILE*   value_file;
        } gpio;
    };
    char            name[LINUX_PTY_NAME_SIZE];
    void            (*cb)();
} fd_t;


typedef struct
{
    persist_storage_t               persist_data;
    uint8_t                         _[2048 - sizeof(persist_storage_t)];
    persist_measurements_storage_t  persist_measurements;
    uint8_t                         __[2048 - sizeof(persist_measurements_storage_t)];
} persist_mem_t;


typedef char pty_buf_t[LINUX_PTY_BUF_SIZ];


static struct pollfd    pfds[LINUX_MAX_PTY * 2];
static uint32_t         nfds;
static fd_t             fd_list[LINUX_MAX_PTY];
static pthread_t        _linux_listener_thread_id;
static persist_mem_t    _linux_persist_mem          = {0};
static bool             _linux_running              = true;


void _linux_sig_handler(int signo)
{
    if (signo == SIGINT)
        _linux_running = false;
}


static void _linux_error(char* fmt, ...)
{
    printf("LINUX ERROR: ");
    va_list v;
    va_start(v, fmt);
    vprintf(fmt, v);
    va_end(v);
    printf("\n");
    exit(-1);
}


static fd_t* _linux_get_fd_handler(int32_t fd)
{
    for (uint32_t i = 0; i < LINUX_MAX_PTY; i++)
    {
        if (fd_list[i].name)
        {
            if ( fd_list[i].pty.master_fd == fd ||
                 fd_list[i].pty.slave_fd  == fd )
                return &fd_list[i];
        }
    }
    return NULL;
}


static void _linux_cleanup_pty(fd_t* fd_handler)
{
    pty_buf_t buf = LINUX_FILE_LOC;
    strncat(buf, fd_handler->name, sizeof(pty_buf_t) - strlen(buf));
    strncat(buf, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t) - strlen(buf));
    if (remove(buf))
        _linux_error("FAIL PTY SYMLINK CLEANUP: %s %"PRIu32, buf, errno);
    if (close(fd_handler->pty.master_fd))
        _linux_error("FAIL PTY FD CLEANUP: %"PRIu32, errno);
    if (close(fd_handler->pty.slave_fd))
        _linux_error("FAIL PTY FD CLEANUP: %"PRIu32, errno);
}


static void _linux_cleanup_fd_handlers(void)
{
    for (uint32_t i = 0; i < LINUX_MAX_PTY; i++)
    {
        if (fd_list[i].name)
        {
            switch(fd_list[i].type)
            {
                case LINUX_FD_TYPE_PTY:
                    _linux_cleanup_pty(&fd_list[i]);
                    break;
                case LINUX_FD_TYPE_IO:
                    break;
            }
        }
    }
}


static void _linux_exit(int err)
{
    printf("Cleaning up before exit...\n");
    _linux_cleanup_fd_handlers();
}


static void _linux_pty_symlink(int fd, char* new_tty_name)
{
    pty_buf_t initial_tty;

    if (ttyname_r(fd, initial_tty, sizeof(initial_tty)))
        _linux_error("FAIL PTY FIND TTY: %"PRIu32, errno);

    if (!access(new_tty_name, F_OK) && remove(new_tty_name))
        printf("FAIL REMOVE OLD SYMLINK: %"PRIu32, errno);

    if (symlink(initial_tty, new_tty_name))
        printf("FAIL SYMLINK TTY: %"PRIu32, errno);
}


static void _linux_setup_pty(fd_t* fd_handler, char* new_tty_name)
{
    if (openpty(&fd_handler->pty.master_fd, &fd_handler->pty.slave_fd, NULL, NULL, NULL))
        _linux_error("FAIL PTY OPEN: %"PRIu32, errno);

    pty_buf_t dir_loc = LINUX_FILE_LOC;

    mode_t mode = S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
    if (mkdir(dir_loc, mode) && errno != EEXIST)
        printf("FAIL CREATE FOLDER: %"PRIu32, errno);

    pty_buf_t pty_loc = LINUX_FILE_LOC;
    strncat(pty_loc, new_tty_name, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    strncat(pty_loc, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    _linux_pty_symlink(fd_handler->pty.slave_fd, pty_loc);
}


void _linux_setup_poll(void)
{
    memset(pfds, 0, sizeof(pfds));
    nfds = 0;
    for (uint32_t i = 0; i < LINUX_MAX_PTY; i++)
    {
        if (fd_list[i].name)
        {
            pfds[i].fd = fd_list[i].pty.master_fd;
            pfds[i].events = POLLIN;
            nfds++;
        }
    }
}


void _linux_iterate(void)
{
    int ready = poll(pfds, nfds, -1);
    if (ready == -1)
        _linux_error("TIMEOUT");
    for (uint32_t i = 0; i < nfds; i++)
    {
        if (pfds[i].revents != 0)
        {
            if (pfds[i].revents & POLLIN)
            {
                pfds[i].revents &= ~POLLIN;

                char buf[10];
                fd_t* fd_handler = _linux_get_fd_handler(pfds[i].fd);
                if (!fd_handler)
                    _linux_error("PTY is NULL pointer");
                int r;
                switch(fd_handler->type)
                {
                    case LINUX_FD_TYPE_PTY:
                        r = read(pfds[i].fd, buf, sizeof(buf) - 1);
                        if (r > 0)
                        {
                            printf("%s << %s\n", fd_handler->name, buf);
                            if (fd_handler->cb)
                                fd_handler->cb(buf, r);
                        }
                        break;
                    case LINUX_FD_TYPE_IO:
                        /* Unsure how this will work */
                        r = read(pfds[i].fd, buf, sizeof(buf) - 1);
                        uint8_t dir = strtoul(buf, NULL, 10);
                        if (fd_handler->cb)
                            fd_handler->cb(buf, dir);
                        break;
                }
            }
        }
    }
}


static void* thread_proc(void* vargp)
{
    while (1)
        _linux_iterate();
    return NULL;
}


bool linux_add_pty(char* name, uint32_t* fd, void (*read_cb)(char* name, unsigned len))
{
    for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* pty = &fd_list[i];
        uint8_t len_1 = strnlen(name, LINUX_PTY_NAME_SIZE);
        uint8_t len_2 = strnlen(pty->name, LINUX_PTY_NAME_SIZE);
        len_1 = len_1 < len_2 ? len_2 : len_1;
        if (strncmp(pty->name, name, len_1) == 0)
            _linux_error("Attempted to add a PTY with same name '%s'", name);
    }
    for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* pty = &fd_list[i];
        if (pty->name[0] == 0)
            continue;
        strncpy(pty->name, name, LINUX_PTY_NAME_SIZE);
        pty->type = LINUX_FD_TYPE_PTY;
        pty->cb = read_cb;
        _linux_setup_pty(pty, name);
        return true;
    }
    _linux_error("No space in array for '%s'", name);
    return false;
}


void platform_init(void)
{
    signal(SIGINT, _linux_sig_handler);
    //_linux_setup_ptys();
    //_linux_setup_w1_sock();
    //_linux_setup_pulse_count(); /* inotify on gpio file and counting accordingly. */
    _linux_setup_poll();
    pthread_create(&_linux_listener_thread_id, NULL, thread_proc, NULL);
}


void platform_deinit(void)
{
    _linux_exit(0);
}


void platform_set_rs485_mode(bool driver_enable)
{
}


void platform_reset_sys(void)
{
    _linux_exit(0);
}


bool linux_add_gpio(char* name, uint32_t* fd, void (*cb)(uint32_t))
{
    return false;
}


static persist_mem_t* _linux_get_persist(void)
{
    FILE* mem_file = fopen(LINUX_PERSIST_FILE_LOC, "rb");
    if (!mem_file)
        return NULL;
    if (fread(&_linux_persist_mem, sizeof(persist_mem_t), 1, mem_file) != sizeof(persist_mem_t))
        return NULL;
    fclose(mem_file);
    return &_linux_persist_mem;
}


persist_storage_t* platform_get_raw_persist(void)
{
    return &_linux_get_persist()->persist_data;
}


persist_measurements_storage_t* platform_get_measurements_raw_persist(void)
{
    return &_linux_get_persist()->persist_measurements;
}


bool platform_persist_commit(persist_storage_t* persist_data, persist_measurements_storage_t* persist_measurements)
{
    FILE* mem_file = fopen(LINUX_PERSIST_FILE_LOC, "wb");
    if (!mem_file)
        return false;
    memcpy(&_linux_persist_mem.persist_data, persist_data, sizeof(persist_storage_t));
    memcpy(&_linux_persist_mem.persist_measurements, persist_measurements, sizeof(persist_measurements_storage_t));
    fwrite(&_linux_persist_mem, sizeof(_linux_persist_mem), 1, mem_file);
    fclose(mem_file);
    return true;
}


void platform_persist_wipe(void)
{
    memset(&_linux_persist_mem, 0, sizeof(persist_mem_t));
    platform_persist_commit(&_linux_persist_mem.persist_data, &_linux_persist_mem.persist_measurements);
}


bool platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page)
{
    return false;
}


void platform_clear_flash_flags(void)
{
    ;
}


bool platform_running(void)
{
    return _linux_running;
}
