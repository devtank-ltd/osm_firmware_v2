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
#include <ctype.h>
#include <sys/timerfd.h>


#include "platform.h"
#include "linux.h"


#define LINUX_FILE_LOC          "/tmp/osm/"
#define LINUX_PTY_BUF_SIZ       64
#define LINUX_PTY_NAME_SIZE     16
#define LINUX_LINE_BUF_SIZ      32
#define LINUX_MAX_NFDS          32

#define LINUX_MASTER_SUFFIX     "_master"
#define LINUX_SLAVE_SUFFIX      "_slave"

#define LINUX_PERSIST_FILE_LOC  LINUX_FILE_LOC"osm.img"
#define LINUX_REBOOT_FILE_LOC   LINUX_FILE_LOC"reboot.dat"


extern int errno;


typedef enum
{
    LINUX_FD_TYPE_PTY,
    LINUX_FD_TYPE_TIMER,
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
        } timer;
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


static struct pollfd    pfds[LINUX_MAX_NFDS];
static uint32_t         nfds;
static pthread_t        _linux_listener_thread_id;
static persist_mem_t    _linux_persist_mem          = {0};
static bool             _linux_running              = true;
static bool             _linux_in_debug             = false;

uint32_t                rcc_ahb_frequency;


static void _linux_systick_cb(char* name, unsigned len);

static fd_t             fd_list[] = {{.type=LINUX_FD_TYPE_PTY,
                                      .name={"UART_DEBUG"},
                                      .cb=uart_debug_cb},
                                     {.type=LINUX_FD_TYPE_PTY,
                                      .name={"UART_LW"},
                                      .cb=uart_lw_cb},
                                     {.type=LINUX_FD_TYPE_PTY,
                                      .name={"UART_HPM"},
                                      .cb=uart_hpm_cb},
                                     {.type=LINUX_FD_TYPE_PTY,
                                      .name={"UART_RS485"},
                                      .cb=uart_rs485_cb},
                                     {.type=LINUX_FD_TYPE_TIMER,
                                      .name={"SYSTICK_TIMER"},
                                      .cb=_linux_systick_cb}};


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
    fprintf(stderr, " (%s)", strerror(errno));
    fprintf(stderr, "\n");
    exit(-1);
}


static fd_t* _linux_get_fd_handler(int32_t fd)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        if (!fd_list[i].name[0])
            continue;

        fd_t* fd_h = &fd_list[i];
        switch (fd_h->type)
        {
            case LINUX_FD_TYPE_PTY:
                if (fd_h->pty.master_fd == fd || fd_h->pty.slave_fd == fd)
                    return fd_h;
                break;
            case LINUX_FD_TYPE_TIMER:
                if (fd_h->timer.fd == fd)
                    return fd_h;
            default:
                continue;
        }
    }
    return NULL;
}


static void _linux_remove_symlink(char name[LINUX_PTY_NAME_SIZE])
{
    pty_buf_t buf = LINUX_FILE_LOC;
    strncat(buf, name, sizeof(pty_buf_t) - strnlen(buf, sizeof(pty_buf_t) -1));
    strncat(buf, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t) - strnlen(buf, sizeof(pty_buf_t) -1));
    if (remove(buf))
        _linux_error("FAIL PTY SYMLINK CLEANUP: %s", buf);
}


static void _linux_pty_symlink(int32_t fd, char* new_tty_name)
{
    pty_buf_t initial_tty;

    if (ttyname_r(fd, initial_tty, sizeof(initial_tty)))
    {
        close(fd);
        _linux_error("FAIL PTY FIND TTY");
    }

    if (!access(new_tty_name, F_OK) && remove(new_tty_name))
    {
        close(fd);
        _linux_error("FAIL REMOVE OLD SYMLINK");
    }

    if (symlink(initial_tty, new_tty_name))
    {
        close(fd);
        _linux_error("FAIL SYMLINK TTY");
    }
}


static void _linux_setup_pty(char name[LINUX_PTY_NAME_SIZE], int32_t* master_fd, int32_t* slave_fd)
{
    if (openpty(master_fd, slave_fd, NULL, NULL, NULL))
        goto bad_exit;
    pty_buf_t dir_loc = LINUX_FILE_LOC;

    mode_t mode = S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
    if (mkdir(dir_loc, mode) && errno != EEXIST)
        goto bad_exit;

    pty_buf_t pty_loc = LINUX_FILE_LOC;
    strncat(pty_loc, name, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    strncat(pty_loc, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    _linux_pty_symlink(*slave_fd, pty_loc);
    return;

bad_exit:
    if (!close(*master_fd))
        _linux_error("Fail to close master fd when couldnt open PTY %s", name);
    if (!close(*slave_fd))
        _linux_error("Fail to close slave fd when couldnt open PTY %s", name);
    _linux_error("Fail to setup PTY.");
}


static void _linux_systick_cb(char* name, unsigned len)
{
    sys_tick_handler();
}


static void _linux_save_fd_file(void)
{
    FILE* osm_reboot_file = fopen(LINUX_REBOOT_FILE_LOC, "w");
    if (!osm_reboot_file)
        _linux_error("Could not make a OSM reboot file.");
    for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (fd->name[0] && isascii(fd->name[0]))
        {
            switch (fd->type)
            {
                case LINUX_FD_TYPE_PTY:
                    fprintf(osm_reboot_file, "%s:%"PRIi32":%"PRIi32"\n", fd->name, fd->pty.master_fd, fd->pty.slave_fd);
                    break;
                default:
                    break;
            }
        }
    }
    fclose(osm_reboot_file);
}


static void _linux_load_fd_file(void)
{
    FILE* osm_reboot_file = fopen(LINUX_REBOOT_FILE_LOC, "r");
    if (!osm_reboot_file)
    {
        //fclose(osm_reboot_file);
        return;
    }
    char line[LINUX_LINE_BUF_SIZ];
    while (fgets(line, LINUX_LINE_BUF_SIZ-1, osm_reboot_file))
    {
        uint16_t len = strnlen(line, LINUX_LINE_BUF_SIZ-1);
        for (unsigned j = 0; j < len; j++)
        {
            if (line[j] == ':')
                line[j] = '\0';
        }
        char* name = line;
        uint16_t name_len = strnlen(name, LINUX_LINE_BUF_SIZ-1);
        if (name_len > LINUX_PTY_NAME_SIZE-1)
            /* Name is too long, ignore. */
            continue;
        char* pos = line + name_len + 1;
        int32_t master_fd = strtol(pos, &pos, 10);
        int32_t slave_fd = strtol(pos+1, NULL, 10);
        for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
        {
            fd_t* fd = &fd_list[i];
            if (!fd->name[0] || !isascii(fd->name[0]))
                continue;
            if (strnlen(fd->name, LINUX_PTY_NAME_SIZE-1) != name_len)
                continue;
            if (strncmp(fd->name, name, name_len) != 0)
                continue;
            fd->pty.master_fd = master_fd;
            fd->pty.slave_fd = slave_fd;
        }
    }
    fclose(osm_reboot_file);
}


static void _linux_rm_fd_file(void)
{
    remove(LINUX_REBOOT_FILE_LOC);
}


static void _linux_setup_systick(int32_t* fd)
{
    *fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct itimerspec timerspec = {0};

    timerspec.it_value.tv_sec  = 1 / 1000;
    timerspec.it_value.tv_nsec = 1000000;
    timerspec.it_interval      = timerspec.it_value;

    if (timerfd_settime(*fd, 0, &timerspec, NULL))
    {
        close(*fd);
        _linux_error("Failed set time of timer file descriptor");
    }
}


static void _linux_setup_fd_handlers(void)
{
    _linux_load_fd_file();
    _linux_rm_fd_file();
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || !isascii(fd->name[0]))
            continue;
        switch(fd->type)
        {
            case LINUX_FD_TYPE_PTY:
                if (!fd->pty.master_fd || !fd->pty.slave_fd)
                    _linux_setup_pty(fd->name, &fd->pty.master_fd, &fd->pty.slave_fd);
                break;
            case LINUX_FD_TYPE_TIMER:
                if (!fd->timer.fd)
                    _linux_setup_systick(&fd->timer.fd);
                break;
            default:
                _linux_error("Unknown type for %s : %"PRIi32".", fd->name, fd->type);
                break;
        }
    }
}


static void _linux_cleanup_fd_handlers(void)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || isascii(fd->name[0]))
            continue;
        switch(fd->type)
        {
            case LINUX_FD_TYPE_PTY:
                if (close(fd->pty.master_fd))
                    _linux_error("Fail close PTY master '%s'.", fd->name);
                if (close(fd->pty.slave_fd))
                    _linux_error("Fail close PTY slave '%s'.", fd->name);
                _linux_remove_symlink(fd->name);
                break;
            case LINUX_FD_TYPE_TIMER:
                if (close(fd_list[i].timer.fd))
                    _linux_error("Fail close TIMER '%s'", fd->name);
                break;
            default:
                _linux_error("Unknown type for %s : %"PRIi32".", fd->name, fd->type);
                break;
        }
    }
}


static void _linux_exit(int err)
{
    fprintf(stdout, "Cleaning up before exit...\n");
    i2c_linux_deinit();
    _linux_cleanup_fd_handlers();
}


bool linux_write_pty(unsigned index, char *data, int size)
{
    return (write(fd_list[index].pty.master_fd, data, size) != 0);
}


static void _linux_setup_poll(void)
{
    memset(pfds, 0, sizeof(pfds));
    nfds = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || !isascii(fd->name[0]))
            continue;
        switch (fd->type)
        {
            case LINUX_FD_TYPE_PTY:
                pfds[i].fd = fd->pty.master_fd;
                pfds[i].events = POLLIN;
                break;
            case LINUX_FD_TYPE_TIMER:
                pfds[i].fd = fd->timer.fd;
                pfds[i].events = POLLIN;
                break;
            default:
                _linux_error("Not implemented.");
                break;
        }
        nfds++;
    }
}


void _linux_iterate(void)
{
    int ready = poll(pfds, nfds, -1);
    if (ready == -1)
        _linux_error("TIMEOUT");
    for (uint32_t i = 0; i < nfds; i++)
    {
        if (pfds[i].revents == 0)
            continue;

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
                case LINUX_FD_TYPE_TIMER:
                {
                    char buf[64];
                    read(fd_handler->timer.fd, buf, 64);
                    if (fd_handler->cb)
                        fd_handler->cb(NULL, 0);
                }
                default:
                    break;
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


void platform_init(void)
{
    fprintf(stdout, "-------------\n");
    fprintf(stdout, "Process ID: %"PRIi32"\n", getpid());
    signal(SIGINT, _linux_sig_handler);
    _linux_setup_fd_handlers();
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
    _linux_save_fd_file();

    fprintf(stdout, "Cleaning up before exit...\n");
    i2c_linux_deinit();

    char link_addr[128];
    char proc_ln[64];
    snprintf(proc_ln, 63, "/proc/%"PRIi32"/exe", getpid());
    ssize_t len = readlink(proc_ln, link_addr, 127);
    link_addr[len] = 0;
    char *const __argv[1] = {0};
    if (execv(link_addr, __argv))
        _linux_error("Could not re-exec firmware");
}


static persist_mem_t* _linux_get_persist(void)
{
    FILE* mem_file = fopen(LINUX_PERSIST_FILE_LOC, "rb");
    if (!mem_file)
        goto bad_exit;
    if (fread(&_linux_persist_mem, sizeof(persist_mem_t), 1, mem_file) != sizeof(persist_mem_t))
        goto bad_exit;
    fclose(mem_file);
    return &_linux_persist_mem;
bad_exit:
    fclose(mem_file);
    return NULL;
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
    {
        fclose(mem_file);
        return false;
    }
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
