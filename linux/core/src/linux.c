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
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <sys/timerfd.h>


#include "platform.h"
#include "linux.h"


#define LINUX_FILE_LOC          "/tmp/osm/"
#define LINUX_PTY_BUF_SIZ       64
#define LINUX_MAX_PTY           32
#define LINUX_PTY_NAME_SIZE     16

#define LINUX_MASTER_SUFFIX     "_master"
#define LINUX_SLAVE_SUFFIX      "_slave"

#define LINUX_PERSIST_FILE_LOC  LINUX_FILE_LOC"osm.img"


extern int errno;


typedef enum
{
    LINUX_FD_TYPE_PTY,
    LINUX_FD_TYPE_IO,
    LINUX_FD_TYPE_SINGLE_FD,
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
        uint32_t single_fd;
        struct
        {
            int32_t fd;
            FILE*   enabled_file;
            FILE*   direction_file;
            FILE*   value_file;
        } gpio;
    };
    char            name[LINUX_PTY_NAME_SIZE];
    void            (*cb)(char * addr, unsigned len);
} fd_t;


typedef struct
{
    persist_measurements_storage_t  persist_measurements;
    persist_storage_t               persist_data;
    uint8_t                         __[2048 - sizeof(persist_storage_t)];
} persist_mem_t;


typedef char pty_buf_t[LINUX_PTY_BUF_SIZ];


static struct pollfd    pfds[LINUX_MAX_PTY * 2];
static uint32_t         nfds;
static fd_t             fd_list[LINUX_MAX_PTY];
static pthread_t        _linux_listener_thread_id;
static persist_mem_t    _linux_persist_mem          = {0};
static bool             _linux_running              = true;
uint32_t rcc_ahb_frequency;


static bool _linux_in_debug = false;


void linux_port_debug(char * fmt, ...)
{
    if (!_linux_in_debug)
        return;

    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
}


void _linux_sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        linux_port_debug("Signal\n");
        _linux_running = false;
    }
}


static void _linux_error(char* fmt, ...)
{
    fprintf(stderr, "LINUX ERROR: ");
    va_list v;
    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    va_end(v);
    fprintf(stderr, "\n");
    exit(-1);
}


static fd_t* _linux_get_fd_handler(int32_t fd)
{
    for (uint32_t i = 0; i < LINUX_MAX_PTY; i++)
    {
        if (fd_list[i].name[0])
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
                case LINUX_FD_TYPE_SINGLE_FD:
                    close(fd_list[i].single_fd);
                    break;
            }
        }
    }
}


static void _linux_exit(int err)
{
    fprintf(stdout, "Cleaning up before exit...\n");
    i2c_linux_deinit();
    _linux_cleanup_fd_handlers();
}


static void _linux_pty_symlink(int fd, char* new_tty_name)
{
    pty_buf_t initial_tty;

    if (ttyname_r(fd, initial_tty, sizeof(initial_tty)))
        _linux_error("FAIL PTY FIND TTY: %"PRIu32, errno);

    if (!access(new_tty_name, F_OK) && remove(new_tty_name))
        fprintf(stderr, "FAIL REMOVE OLD SYMLINK: %"PRIu32, errno);

    if (symlink(initial_tty, new_tty_name))
        fprintf(stderr, "FAIL SYMLINK TTY: %"PRIu32, errno);
}


static void _linux_setup_pty(fd_t* fd_handler, char* new_tty_name)
{
    if (openpty(&fd_handler->pty.master_fd, &fd_handler->pty.slave_fd, NULL, NULL, NULL))
        _linux_error("FAIL PTY OPEN: %"PRIu32, errno);

    pty_buf_t dir_loc = LINUX_FILE_LOC;

    mode_t mode = S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
    if (mkdir(dir_loc, mode) && errno != EEXIST)
        fprintf(stderr, "FAIL CREATE FOLDER: %"PRIu32, errno);

    pty_buf_t pty_loc = LINUX_FILE_LOC;
    strncat(pty_loc, new_tty_name, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    strncat(pty_loc, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    _linux_pty_symlink(fd_handler->pty.slave_fd, pty_loc);
}


static void _linux_systick_cb(char* name, unsigned len)
{
    sys_tick_handler();
}


void _linux_setup_systick(void)
{
    int systick_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct itimerspec timerspec = {0};

    timerspec.it_value.tv_sec  = 1 / 1000;
    timerspec.it_value.tv_nsec = 1000000;
    timerspec.it_interval      = timerspec.it_value;

    if (timerfd_settime(systick_timerfd, 0, &timerspec, NULL))
    {
        close(systick_timerfd);
        _linux_error("Failed set time of timer file descriptor: %s", strerror(errno));
    }

    char name[LINUX_PTY_NAME_SIZE] = "systick_timer";
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
        if (pty->name[0] != 0)
            continue;
        strncpy(pty->name, name, LINUX_PTY_NAME_SIZE);
        pty->type = LINUX_FD_TYPE_SINGLE_FD;
        pty->cb = _linux_systick_cb;
        pty->single_fd = systick_timerfd;
        return;
    }
    _linux_error("No space in array for '%s'", name);
    return;
}


void _linux_setup_poll(void)
{
    memset(pfds, 0, sizeof(pfds));
    nfds = 0;
    for (uint32_t i = 0; i < LINUX_MAX_PTY; i++)
    {
        fd_t* fd = &fd_list[i];
        if (fd->name)
        {
            switch (fd->type)
            {
                case LINUX_FD_TYPE_PTY:
                    pfds[i].fd = fd->pty.master_fd;
                    pfds[i].events = POLLIN;
                    break;
                case LINUX_FD_TYPE_SINGLE_FD:
                    pfds[i].fd = fd->single_fd;
                    pfds[i].events = POLLIN;
                    break;
                default:
                    _linux_error("Not implemented.");
                    break;
            }
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
                char c;

                fd_t* fd_handler = _linux_get_fd_handler(pfds[i].fd);
                if (!fd_handler)
                    _linux_error("PTY is NULL pointer");
                int r;
                switch(fd_handler->type)
                {
                    case LINUX_FD_TYPE_PTY:
                        r = read(pfds[i].fd, &c, 1);
                        if (r <= 0)
                            break;
                        if (r == 1)
                        {
                            linux_port_debug("%s << %c\n", fd_handler->name, c);
                            if (fd_handler->cb)
                                fd_handler->cb(&c, 1);
                        }
                        break;
                    case LINUX_FD_TYPE_IO:
                    {
                        char buf[10];
                        /* Unsure how this will work */
                        r = read(pfds[i].fd, buf, sizeof(buf) - 1);
                        uint8_t dir = strtoul(buf, NULL, 10);
                        if (fd_handler->cb)
                            fd_handler->cb(buf, dir);
                        break;
                    }
                    case LINUX_FD_TYPE_SINGLE_FD:
                    {
                        char buf[64];
                        read(fd_handler->single_fd, buf, 64);
                        if (fd_handler->cb)
                            fd_handler->cb(NULL, 0);
                    }
                    default:
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
        if (pty->name[0] != 0)
            continue;
        strncpy(pty->name, name, LINUX_PTY_NAME_SIZE);
        pty->type = LINUX_FD_TYPE_PTY;
        pty->cb = read_cb;
        _linux_setup_pty(pty, name);
        *fd = pty->pty.master_fd;
        return true;
    }
    _linux_error("No space in array for '%s'", name);
    return false;
}


void platform_init(void)
{
    fprintf(stdout, "Process ID: %"PRIi32"\n", getpid());
    signal(SIGINT, _linux_sig_handler);
    uarts_linux_setup();
    _linux_setup_systick();
    _linux_setup_poll();
    pthread_create(&_linux_listener_thread_id, NULL, thread_proc, NULL);
}


void platform_watchdog_init(uint32_t ms)
{
    ;
}


void platform_watchdog_reset(void)
{
    ;
}


void platform_blink_led_init(void)
{
    ;
}


void platform_blink_led_toggle(void)
{
    ;
}


void platform_deinit(void)
{
    _linux_exit(0);
}


void platform_setup_adc(adc_setup_config_t* config)
{
}


void platform_adc_set_regular_sequence(uint8_t num_channels, adcs_type_t* channels)
{
}


void platform_adc_start_conversion_regular(void)
{
}


void platform_adc_power_off(void)
{
}


void platform_adc_set_num_data(unsigned num_data)
{
}


void platform_set_rs485_mode(bool driver_enable)
{
}


void platform_reset_sys(void)
{
    _linux_error("platform_reset_sys\n");
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
    persist_mem_t* persist = _linux_get_persist();
    if (!persist)
        return NULL;
    return &persist->persist_data;
}


persist_measurements_storage_t* platform_get_measurements_raw_persist(void)
{
    persist_mem_t* persist = _linux_get_persist();
    if (!persist)
        return NULL;
    return &persist->persist_measurements;
}


bool platform_persist_commit(persist_storage_t* persist_data, persist_measurements_storage_t* persist_measurements)
{
    FILE* mem_file = fopen(LINUX_PERSIST_FILE_LOC, "wb");
    if (!mem_file)
        return false;
    if (persist_data != &_linux_persist_mem.persist_data)
        memcpy(&_linux_persist_mem.persist_data, persist_data, sizeof(persist_storage_t));
    if (persist_measurements != &_linux_persist_mem.persist_measurements)
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


void platform_hpm_enable(bool enable)
{
    ;
}
