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
#include <time.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <libgen.h>
#include <linux/limits.h>
#include <spawn.h>
#include <fcntl.h>
#include <netinet/in.h>


#include "platform.h"
#include "linux.h"
#include "common.h"
#include "uarts.h"
#include "persist_config.h"
#include "log.h"
#include "measurements.h"
#include "platform_model.h"
#include "pinmap.h"

#define LINUX_PTY_BUF_SIZ       64
#define LINUX_LINE_BUF_SIZ      32
#define LINUX_MAX_NFDS          32

#define LINUX_MASTER_SUFFIX     "_master"
#define LINUX_SLAVE_SUFFIX      "_slave"

#define LINUX_PERSIST_FILE_LOC  "osm.img"
#define LINUX_REBOOT_FILE_LOC   "reboot.dat"
#define LINUX_REBOOT_FILE_TIMEOUT_S 60

#define LINUX_FD_SAVE_FMT "%s %"PRIi32" %"PRIi32" %u\n"

#define LINUX_DEFAULT_SOCKET_PORT       10240


static int _linux_socket_port = LINUX_DEFAULT_SOCKET_PORT;
bool linux_has_reset = false;


typedef enum
{
    LINUX_FD_TYPE_PTY,
    LINUX_FD_TYPE_TIMER,
    LINUX_FD_TYPE_EVENT,
    LINUX_FD_TYPE_SOCKET_SERVER,
    LINUX_FD_TYPE_SOCKET_CLIENT,
} linux_fd_type_t;

typedef struct fd_t fd_t;

struct fd_t
{
    linux_fd_type_t type;
    union
    {
        struct
        {
            int32_t master_fd;
            int32_t slave_fd;
            unsigned uart;
        } pty;
        struct
        {
            int32_t fd;
        } timer;
        struct
        {
            int32_t fd;
        } event;
        struct
        {
            int32_t fd;
            unsigned uart;
            fd_t* client;
        } socket_server;
        struct
        {
            int32_t fd;
            fd_t* server;
        } socket_client;
    };
    char            name[LINUX_PTY_NAME_SIZE];
    void            (*cb)();
};


typedef struct
{
    persist_measurements_storage_t  persist_measurements;
    persist_storage_t               persist_data;
    uint8_t                         __[2048 - sizeof(persist_storage_t)];
}  __attribute__((__packed__)) persist_mem_t;


typedef char pty_buf_t[LINUX_PTY_BUF_SIZ];


static struct pollfd    pfds[LINUX_MAX_NFDS];
static uint32_t         nfds;
static pthread_t        _linux_listener_thread_id;
static persist_mem_t    _linux_persist_mem          = {0};
static volatile bool    _linux_running              = true;
static bool             _linux_in_debug             = false;
volatile bool           linux_threads_deinit        = false;
static int64_t          _linux_boot_time_us         = 0;

static pthread_cond_t  _sleep_cond  =  PTHREAD_COND_INITIALIZER;
static pthread_mutex_t _sleep_mutex =  PTHREAD_MUTEX_INITIALIZER;

static bool _ios_enabled[IOS_COUNT] = {0};



static fd_t             fd_list[LINUX_MAX_NFDS] = {{.type=LINUX_FD_TYPE_PTY,
                                                    .name={"UART_DEBUG"},
                                                    .pty = {.uart = CMD_UART},
                                                    .cb=linux_uart_proc
                                                    },
                                                   {.type=LINUX_FD_TYPE_PTY,
                                                    .name={"UART_COMMS"},
                                                    .pty = {.uart = COMMS_UART},
                                                    .cb=linux_uart_proc},
                                                   {.type=LINUX_FD_TYPE_EVENT,
                                                    .name={"ADC_GEN_EVENT"},
                                                    .cb=linux_adc_generate}};


uint32_t platform_get_frequency(void)
{
    static uint32_t freq = 0;
    if (!freq)
    {
        FILE * f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
        if (f)
        {
            fscanf(f, PRIu32, &freq);
            fclose(f);
        }
    }
    return freq;
}


void linux_port_debug(char * fmt, ...)
{
    if (!_linux_in_debug)
        return;

    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "%010u:", (unsigned)get_since_boot_ms());
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}


static void _linux_sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        fprintf(stderr, "Caught signal, exiting gracefully.\n");
        _linux_running = false;
        linux_awaken();
    }
    else if (signo == SIGUSR1)
    {
        _linux_in_debug = !_linux_in_debug;
    }
    else
    {
        fprintf(stderr, "Fatal Signal : %s\n", strsignal(signo));
        model_linux_close_fakes();
        exit(-1);
    }
}


static void _linux_error(char * fmt, va_list v, int error)
{
    fprintf(stderr, "LINUX ERROR: ");
    vfprintf(stderr, fmt, v);
    va_end(v);
    fprintf(stderr, " (%s)", strerror(errno));
    fprintf(stderr, "\n");
    model_linux_close_fakes();
    exit(-1);
}


void linux_error(char* fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    _linux_error(fmt, v, errno);
    va_end(v);
}


void linux_error2(int error, char* fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    _linux_error(fmt, v, error);
    va_end(v);
}


unsigned linux_spawn(const char * rel_path)
{
    static char full_path[PATH_MAX];
    memset(full_path, 0, PATH_MAX);
    if (readlink("/proc/self/exe", full_path, PATH_MAX) < 0)
    {
        linux_error("Failed start spawn : %s", rel_path);
        return 0;
    }
    const char *exec_dir = dirname(full_path);
    unsigned used = PATH_MAX-strlen(exec_dir);
    strncat(full_path, "/", used);
    strncat(full_path, rel_path, used - 1);

    pid_t pid;
    char *argv[] = {full_path, NULL};
    int r = posix_spawn(&pid, full_path, NULL, NULL, argv, environ);
    if (r)
    {
        linux_error("Spawn failed %s : %s", full_path, strerror(r));
        return 0;
    }
    linux_port_debug("Spawned %s pid: %i", full_path, pid);
    return (unsigned)pid;
}


int64_t linux_get_current_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts))
        linux_error("Failed to get the realtime of the clock.");

    return (int64_t)(ts.tv_nsec / 1000LL) + (int64_t)(ts.tv_sec * 1000000);
}


static fd_t* _linux_get_fd_handler(int32_t fd)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        if (!fd_list[i].name[0])
            return NULL;

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
                break;
            case LINUX_FD_TYPE_EVENT:
                if (fd_h->event.fd == fd)
                    return fd_h;
                break;
            case LINUX_FD_TYPE_SOCKET_SERVER:
                if (fd_h->socket_server.fd == fd)
                    return fd_h;
                break;
            case LINUX_FD_TYPE_SOCKET_CLIENT:
                if (fd_h->socket_client.fd == fd)
                    return fd_h;
                break;
            default:
                return NULL;
        }
    }
    return NULL;
}


static void _linux_remove_symlink(char name[LINUX_PTY_NAME_SIZE])
{

    pty_buf_t buf;
    char* file_loc = ret_static_file_location();
    unsigned len = strnlen(file_loc, sizeof(pty_buf_t) -1);
    strncpy(buf, file_loc, len + 1);
    strncat(buf, name, sizeof(pty_buf_t) - strnlen(buf, sizeof(pty_buf_t) -1));
    strncat(buf, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t) - strnlen(buf, sizeof(pty_buf_t) -1));
    if (remove(buf))
        linux_error("FAIL PTY SYMLINK CLEANUP: %s", buf);
    linux_port_debug("Removed symlink \"%s\"", buf);
}


static void _linux_pty_symlink(int32_t fd, char* new_tty_name)
{
    pty_buf_t initial_tty;

    if (ttyname_r(fd, initial_tty, sizeof(initial_tty)))
    {
        close(fd);
        linux_error("FAIL - PTY FIND TTY FOR \"%s\"", new_tty_name);
    }

    if (access(new_tty_name, F_OK) == 0)
    {
        close(fd);
        linux_error("FAIL - SYMLINK \"%s\" ALREADY EXISTS", new_tty_name);
    }

    if (!access(new_tty_name, F_OK) && remove(new_tty_name))
    {
        close(fd);
        linux_error("FAIL - REMOVE OLD SYMLINK \"%s\"", new_tty_name);
    }

    if (symlink(initial_tty, new_tty_name))
    {
        close(fd);
        linux_error("FAIL - SYMLINK TTY \"%s\"", new_tty_name);
    }
}


static void _linux_setup_pty(char name[LINUX_PTY_NAME_SIZE], int32_t* master_fd, int32_t* slave_fd)
{
    if (openpty(master_fd, slave_fd, NULL, NULL, NULL))
        goto bad_exit;
    pty_buf_t dir_loc;
    char* file_loc = ret_static_file_location();
    unsigned len = strnlen(file_loc, sizeof(pty_buf_t) -1);
    strncpy(dir_loc, file_loc, len + 1);

    mode_t mode = S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
    if (mkdir(dir_loc, mode) && errno != EEXIST)
        goto bad_exit;

    pty_buf_t pty_loc;
    unsigned leng = strnlen(file_loc, sizeof(pty_buf_t) -1);
    strncpy(pty_loc, file_loc, leng + 1);

    strncat(pty_loc, name, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    strncat(pty_loc, LINUX_SLAVE_SUFFIX, sizeof(pty_buf_t)-strnlen(pty_loc, sizeof(pty_buf_t)));
    _linux_pty_symlink(*slave_fd, pty_loc);
    return;

bad_exit:
    if (master_fd && !close(*master_fd))
        linux_error("Fail to close master fd when couldnt open PTY %s", name);
    if (slave_fd && !close(*slave_fd))
        linux_error("Fail to close slave fd when couldnt open PTY %s", name);
    linux_error("Fail to setup PTY.");
}


static void _linux_save_fd_file(void)
{
    char osm_reboot_loc[LOCATION_LEN];
    concat_osm_location(osm_reboot_loc, LOCATION_LEN, LINUX_REBOOT_FILE_LOC);
    FILE* osm_reboot_file = fopen(osm_reboot_loc, "w");
    if (!osm_reboot_file)
        linux_error("Could not make a OSM reboot file.");
    for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (fd->name[0] && isascii(fd->name[0]))
        {
            switch (fd->type)
            {
                case LINUX_FD_TYPE_PTY:
                    fprintf(osm_reboot_file, LINUX_FD_SAVE_FMT, fd->name, fd->pty.master_fd, fd->pty.slave_fd, fd->pty.uart);
                    linux_port_debug("Save FD %i \"%s\" UART:%u", i, fd->name, fd->pty.uart);
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
    char osm_reboot_loc[LOCATION_LEN];
    concat_osm_location(osm_reboot_loc, LOCATION_LEN, LINUX_REBOOT_FILE_LOC);
    FILE* osm_reboot_file = fopen(osm_reboot_loc, "r");
    if (!osm_reboot_file)
    {
        linux_port_debug("No saved file descriptors.");
        return;
    }
    /* Check last modified time */
    struct stat attr;
    time_t utc_now = time( NULL );
    stat(osm_reboot_loc, &attr);
    if (utc_now - attr.st_mtime > LINUX_REBOOT_FILE_TIMEOUT_S)
    {
        linux_port_debug("Reboot file is outdated.");
        fclose(osm_reboot_file);
        return;
    }
    linux_port_debug("Loading saved file descriptors.");
    char line[LINUX_LINE_BUF_SIZ];
    while (fgets(line, LINUX_LINE_BUF_SIZ-1, osm_reboot_file))
    {
        char  name[LINUX_PTY_NAME_SIZE] = {0};
        int32_t master_fd;
        int32_t slave_fd;
        unsigned uart;
        if (sscanf(line, LINUX_FD_SAVE_FMT, name, &master_fd, &slave_fd, &uart) != 4)
        {
            linux_error("Failed to read saved FD line!");
            continue;
        }
        unsigned name_len = strnlen(name, LINUX_PTY_NAME_SIZE-1);

        bool found = false;
        int next_slot = -1;
        for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
        {
            fd_t* fd = &fd_list[i];
            if (!fd->name[0] || !isascii(fd->name[0]))
            {
                if (next_slot == -1)
                    next_slot = i;
                continue;
            }
            if (strnlen(fd->name, LINUX_PTY_NAME_SIZE-1) != name_len)
                continue;
            if (strncmp(fd->name, name, name_len) != 0)
                continue;
            fd->pty.master_fd = master_fd;
            fd->pty.slave_fd = slave_fd;
            fd->pty.uart = uart;
            linux_port_debug("Saved FD %u \"%s\" UART:%u found/restored", i, fd->name, uart);
            found = true;
        }
        if (!found)
        {
            if (next_slot != -1)
            {
                fd_t* fd = &fd_list[next_slot];
                memcpy(fd->name, name, name_len+1);
                fd->type = LINUX_FD_TYPE_PTY;
                fd->cb = linux_uart_proc;
                fd->pty.uart = uart;
                fd->pty.master_fd = master_fd;
                fd->pty.slave_fd = slave_fd;
                linux_port_debug("Saved FD %d \"%s\" UART:%u restored", next_slot, fd->name, uart);
            }
        }
    }
    fclose(osm_reboot_file);
    linux_has_reset = true;
}


static void _linux_rm_fd_file(void)
{
    char osm_reboot_loc[LOCATION_LEN];
    concat_osm_location(osm_reboot_loc, LOCATION_LEN, LINUX_REBOOT_FILE_LOC);
    remove(osm_reboot_loc);
}


static void _linux_setup_systick(int32_t* fd)
{
    *fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct itimerspec timerspec = {0};

    timerspec.it_value.tv_sec  = 0;
    timerspec.it_value.tv_nsec = 1000000;
    timerspec.it_interval      = timerspec.it_value;

    if (timerfd_settime(*fd, 0, &timerspec, NULL))
    {
        close(*fd);
        linux_error("Failed set time of timer file descriptor");
    }
}


static void _linux_kick_event(int fd)
{
    uint64_t v = 0xC0FFEE;
    write(fd, &v, sizeof(uint64_t));
    linux_port_debug("Requested ADCs.");
}


bool linux_kick_adc_gen(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        switch(fd->type)
        {
            case LINUX_FD_TYPE_EVENT:
                if (strcmp(fd->name, "ADC_GEN_EVENT") == 0)
                {
                    _linux_kick_event(fd->event.fd);
                    return true;
                }
                break;
            default:
                continue;
        }
    }
    return false;
}


static void _linux_setup_event(int* fd)
{
    int _fd_ = eventfd(0, EFD_CLOEXEC);
    if (_fd_ < 0)
        linux_error("Failed to create event fd.");
    *fd = _fd_;
}


static void _linux_setup_socket_server(int* fd)
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        linux_error("Error opening socket");
    }
    struct sockaddr_in addr;
    addr.sin_port = htons(_linux_socket_port);
    addr.sin_addr.s_addr = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;

    if(bind(socket_fd, (struct sockaddr *)&addr,sizeof(struct sockaddr_in) ) < 0)
    {
        linux_error("Error binding socket");
    }

    if (listen(socket_fd, 1) < 0)
    {
        linux_error("Error listening socket");
    }
    *fd = socket_fd;
}


static void _linux_setup_fd_handlers(void)
{
    _linux_load_fd_file();
    _linux_rm_fd_file();
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || !isascii(fd->name[0]))
            break; /*End of used slots.*/
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
            case LINUX_FD_TYPE_EVENT:
                if (!fd->event.fd)
                    _linux_setup_event(&fd->event.fd);
                break;
            case LINUX_FD_TYPE_SOCKET_SERVER:
                if (!fd->socket_server.fd)
                    _linux_setup_socket_server(&fd->socket_server.fd);
                break;
            case LINUX_FD_TYPE_SOCKET_CLIENT:
                break;
            default:
                linux_error("Unknown type for %s : %"PRIi32".", fd->name, fd->type);
                break;
        }
    }
}


static void _linux_cleanup_fd_handlers(void)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || !isascii(fd->name[0]))
            continue;
        switch(fd->type)
        {
            case LINUX_FD_TYPE_PTY:
                if (close(fd->pty.master_fd))
                    linux_error("Fail close PTY master '%s'.", fd->name);
                if (close(fd->pty.slave_fd))
                    linux_error("Fail close PTY slave '%s'.", fd->name);
                _linux_remove_symlink(fd->name);
                break;
            case LINUX_FD_TYPE_TIMER:
                if (close(fd->timer.fd))
                    linux_error("Fail close TIMER '%s'", fd->name);
                break;
            case LINUX_FD_TYPE_EVENT:
                if (close(fd->event.fd))
                    linux_error("Fail close EVENT '%s'", fd->name);
                break;
            case LINUX_FD_TYPE_SOCKET_SERVER:
                if (fd->socket_server.client &&
                    close(fd->socket_server.client->socket_client.fd))
                    linux_error("Fail close SOCKET client '%s'", fd->name);
                memset(fd->socket_server.client, 0, sizeof(fd_t));
                fd->socket_server.client = NULL;
                if (close(fd->socket_server.fd))
                    linux_error("Fail close SOCKET server '%s'", fd->name);
                break;
            case LINUX_FD_TYPE_SOCKET_CLIENT:
                if (close(fd->socket_client.fd))
                    linux_error("Fail close SOCKET client '%s'", fd->name);
                break;
            default:
                linux_error("Unknown type for %s : %"PRIi32".", fd->name, fd->type);
                break;
        }
    }
}


static void _linux_exit(int err)
{
    fprintf(stdout, "Collecting threads...\n");
    linux_threads_deinit = true;
    pthread_join(_linux_listener_thread_id, NULL);
    fprintf(stdout, "Cleaning up before exit...\n");
    i2c_linux_deinit();
    w1_linux_deinit();
    _linux_cleanup_fd_handlers();
    model_linux_close_fakes();
    fprintf(stdout, "Finished.\n");
}


static bool _safe_write(int fd, const void* data, size_t len, int64_t max_usecs)
{
    int64_t now = linux_get_current_us();
    size_t written = 0;
    while(written < len)
    {
        int r = write(fd, ((const uint8_t*)data) + written, len - written);
        if (r == -1)
        {
            if (errno == EAGAIN)
                continue;
            linux_port_debug("Safe write failed : %s", strerror(errno));
            return false;
        }
        else written += r;
        if (written != len && (linux_get_current_us() - now) > max_usecs)
        {
            linux_port_debug("Safe write timed out.");
            return false;
        }
    }
    if (isatty(fd))
    {
        if (!tcdrain(fd))
        {
            return true;
        }
    }
    else if (!syncfs(fd))
    {
        return true;
    }
    linux_port_debug("Failed to sync fd:%i : %s", fd, strerror(errno));
    return false;
}


bool linux_write_pty(unsigned uart, const char *data, unsigned size)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd_handler = &fd_list[i];
        if (!fd_handler->name[0] || !isascii(fd_handler->name[0]))
        {
            continue;
        }
        if ((fd_handler->type == LINUX_FD_TYPE_PTY && fd_handler->pty.uart == uart) ||
            (fd_handler->type == LINUX_FD_TYPE_SOCKET_CLIENT && fd_handler->socket_client.server->socket_server.uart == uart))
        {
            if (uart != CMD_UART)
            {
                for(unsigned n = 0; n < size; n++)
                {
                    char c = data[n];
                    if (isgraph(c))
                        linux_port_debug("%s(%u) >> '%c' (0x%02"PRIx8")", fd_handler->name, uart, c, (uint8_t)c);
                    else
                        linux_port_debug("%s(%u) >> [0x%02"PRIx8"]", fd_handler->name, uart, (uint8_t)c);
                }
            }
            return _safe_write(fd_handler->pty.master_fd, data, size, 500);
        }
    }
    return false;
}


static void _linux_setup_poll(void)
{
    memset(pfds, 0, sizeof(pfds));
    nfds = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || !isascii(fd->name[0]))
            return;
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
            case LINUX_FD_TYPE_EVENT:
                pfds[i].fd = fd->event.fd;
                pfds[i].events = POLLIN;
                break;
            case LINUX_FD_TYPE_SOCKET_SERVER:
                pfds[i].fd = fd->socket_server.fd;
                pfds[i].events = POLLIN;
                break;
            case LINUX_FD_TYPE_SOCKET_CLIENT:
                pfds[i].fd = fd->socket_client.fd;
                pfds[i].events = POLLIN;
                break;
            default:
                linux_error("Not implemented.");
                break;
        }
        nfds++;
    }
}


bool peripherals_add_uart_tty_bridge(char * pty_name, unsigned uart)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(fd_list); i++)
    {
        fd_t* fd = &fd_list[i];
        if (!fd->name[0] || !isascii(fd->name[0]))
        {
            if (pty_name[0] == '/') /*Is it a path to an existing TTY? */
            {
                char * pty_path = pty_name;
                linux_port_debug("UART %u given path %s", uart, pty_path);
                pty_name = basename(pty_name);
                int fd_id = open(pty_path, O_RDWR | O_NOCTTY | O_SYNC);
                if (fd_id < 0)
                {
                    linux_error("Failed to open %s", pty_path);
                    return false;
                }
                else
                {
                    unsigned name_len = strlen(pty_name);

                    if (name_len > 6 && strncmp(pty_name + name_len - 6, "_slave", 6) == 0)
                        name_len -= 6;

                    if (name_len > (LINUX_PTY_NAME_SIZE - 1))
                    {
                        linux_error2(EINVAL, "PTY name %s too long", pty_name);
                        return false;
                    }
                    strncpy(fd->name, pty_name, name_len);
                    fd->type = LINUX_FD_TYPE_PTY;
                    fd->pty.uart = uart;
                    fd->cb = linux_uart_proc;
                    fd->pty.slave_fd = -1;
                    fd->pty.master_fd = fd_id;
                    linux_port_debug("UART %u %s for %s", uart, fd->name, pty_path);
                }
            }
            else
            {
                if (strlen(pty_name) >    (LINUX_PTY_NAME_SIZE - 1))
                {
                    linux_error2(EINVAL, "PTY name %s too long", pty_name);
                    return false;
                }

                char tty_path[128];
                strcpy(tty_path, ret_static_file_location());
                strcat(tty_path, pty_name);
                strcat(tty_path, LINUX_SLAVE_SUFFIX);
                if (access(tty_path, F_OK) == 0)
                {
                    return true;
                }

                strncpy(fd->name, pty_name, LINUX_PTY_NAME_SIZE);
                fd->type = LINUX_FD_TYPE_PTY;
                fd->pty.uart = uart;
                fd->cb = linux_uart_proc;
                _linux_setup_pty(fd->name, &fd->pty.master_fd, &fd->pty.slave_fd);
                linux_port_debug("UART %u is now %s%s_slave", uart, ret_static_file_location(), pty_name);
            }
            _linux_setup_poll();
            return true;
        }
    }
    linux_error2(ENOBUFS, "No spare PTY slot.");
    return false;
}


static int _linux_get_fd_peek(int fd)
{
    int peek = 0;
    if (ioctl(fd, FIONREAD, &peek) < 0)
        return -1;
    return peek;
}


void _linux_iterate(void)
{
    int ready = poll(pfds, nfds, 1000);
    if (linux_threads_deinit)
        return;
    if (ready == -1 && _linux_running)
        linux_error("TIMEOUT");
    for (uint32_t i = 0; i < nfds; i++)
    {
        if (pfds[i].revents == 0)
            continue;

        if (pfds[i].revents & POLLIN)
        {
            pfds[i].revents &= ~POLLIN;

            fd_t* fd_handler = _linux_get_fd_handler(pfds[i].fd);
            if (!fd_handler)
                linux_error("PTY is NULL pointer");
            switch(fd_handler->type)
            {
                case LINUX_FD_TYPE_PTY:
                {
                    int len = _linux_get_fd_peek(pfds[i].fd);
                    if (len > 1023)
                        len = 1023;
                    if (len <= 0)
                        break;
                    if (len == 1)
                    {
                        char c;
                        int r = read(pfds[i].fd, &c, len);
                        if (r != len)
                            linux_port_debug("Peak does not equal len");
                        if (r <= 0)
                            break;
                        if (isgraph(c))
                            linux_port_debug("%s << '%c' (0x%02"PRIx8")", fd_handler->name, c, (uint8_t)c);
                        else
                            linux_port_debug("%s << [0x%02"PRIx8"]", fd_handler->name, (uint8_t)c);
                        if (fd_handler->cb)
                            fd_handler->cb(fd_handler->pty.uart, &c, 1);
                    }
                    else
                    {
                        char buf[1024];
                        int r = read(pfds[i].fd, buf, len);
                        if (r != len)
                            linux_port_debug("Peak does not equal len");
                        if (r <= 0)
                            break;
                        if (fd_handler->cb)
                            fd_handler->cb(fd_handler->pty.uart, buf, r);
                        char out_buf[1024];
                        char* pos = out_buf;
                        for (int i = 0; i < r; i++)
                        {
                            if ((isgraph(buf[i]) || buf[i] == ' ') && buf[i] != 0)
                            {
                                *pos = buf[i];
                                pos++;
                            }
                            else
                            {
                                snprintf(pos, 1023 + out_buf - pos, "[0x%02"PRIu8"]", buf[i]);
                                pos += 6;
                            }
                        }
                        *pos = 0;
                        linux_port_debug("%s << '%s'", fd_handler->name, out_buf);
                    }
                    break;
                }
                case LINUX_FD_TYPE_TIMER:
                {
                    char buf[64];
                    read(fd_handler->timer.fd, buf, 64);
                    if (fd_handler->cb)
                        fd_handler->cb();
                    break;
                }
                case LINUX_FD_TYPE_EVENT:
                {
                    uint64_t v;
                    read(fd_handler->event.fd, &v, sizeof(uint64_t));
                    linux_port_debug("Received request ADCs.");
                    if (fd_handler->cb)
                        fd_handler->cb(fd_handler->event.fd);
                    break;
                }
                case LINUX_FD_TYPE_SOCKET_SERVER:
                {
                    struct sockaddr_in client_address;
                    socklen_t client_len = sizeof(client_address);
                    int client_sockfd = accept(fd_handler->socket_server.fd, (struct sockaddr*)&client_address, &client_len);
                    if (client_sockfd < 0)
                    {
                        linux_error("Client socket connection accept failed.");
                        break;
                    }
                    if (fd_handler->socket_server.client)
                    {
                        close(client_sockfd);
                        linux_error("Another socket connection attempted?");
                        break;
                    }
                    for (unsigned i = 0; i < LINUX_MAX_NFDS; i++)
                    {
                        fd_t* tfdh = &fd_list[i];
                        if (tfdh == fd_handler)
                            continue;
                        if (!tfdh->name[0])
                        {
                            strncpy(tfdh->name, fd_handler->name, LINUX_PTY_NAME_SIZE-1);
                            unsigned rem_len = LINUX_PTY_NAME_SIZE - strnlen(tfdh->name, LINUX_PTY_NAME_SIZE-1) - 1;
                            strncat(tfdh->name, "_CLIENT", rem_len);
                            tfdh->name[LINUX_PTY_NAME_SIZE-1] = 0;
                            tfdh->type = LINUX_FD_TYPE_SOCKET_CLIENT;
                            tfdh->socket_client.fd = client_sockfd;
                            tfdh->socket_client.server = fd_handler;
                            tfdh->cb = linux_uart_proc;
                            fd_handler->socket_server.client = tfdh;
                            _linux_setup_poll();
                            linux_port_debug("SOCKET CLIENT CONNECTED");
                            return;
                        }
                    }
                    close(client_sockfd);
                    linux_error("Client socket connection failed, no spare slot.");
                    break;
                }
                case LINUX_FD_TYPE_SOCKET_CLIENT:
                {
                    int total = 0;
                    int r = 1;
                    int fd = fd_handler->socket_client.fd;
                    while (r)
                    {
                        char c;
                        r = read(fd, &c, 1);
                        if (r == 1)
                        {
                            int uart = fd_handler->socket_server.uart;
                            if (uart != CMD_UART)
                            {
                                if (isgraph(c))
                                    linux_port_debug("%s(%u) << '%c' (0x%02"PRIx8")", fd_handler->name, uart, c, (uint8_t)c);
                                else
                                    linux_port_debug("%s(%u) << [0x%02"PRIx8"]", fd_handler->name, uart, (uint8_t)c);
                            }
                            if (fd_handler->cb)
                            {
                                fd_handler->cb(uart, &c, 1);
                            }
                        }
                        total += r;
                    }
                    if (total == 0)
                    {
                        close(fd);
                        fd_handler->socket_client.server->socket_server.client = NULL;
                        memset(fd_handler, 0, sizeof(fd_t));
                        _linux_setup_poll();
                        linux_port_debug("DISCONNECTED CLIENT");
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}


void* thread_proc(void* vargp)
{
    while (!linux_threads_deinit)
        _linux_iterate();
    return NULL;
}

char* ret_static_file_location(void)
{
    char * loc = getenv("LOC");
    if (!loc)
        return "/tmp/osm/";
    return loc;
}

void platform_init(void)
{
    _linux_boot_time_us = linux_get_current_us();

    if (setvbuf(stdout, NULL, _IOLBF, 1024) < 0)
        fprintf(stderr, "ERROR : %s\n", strerror(errno));

    if (getenv("DEBUG"))
    {
        _linux_in_debug = true;
        linux_port_debug("Enabled Linux Debug");
    }

    char* port = getenv("USE_PORT");
    if (port)
    {
        int l_port = strtol(port, NULL, 10);
        if (l_port < 1000)
        {
            linux_port_debug("Bad port, using default %d", _linux_socket_port);
        }
        else
        {
            _linux_socket_port = l_port;
        }
        for (unsigned i = 0; i < LINUX_MAX_NFDS; i++)
        {
            fd_t* fd = &fd_list[i];
            if (fd->type == LINUX_FD_TYPE_PTY && strcmp(fd->name, "UART_DEBUG") == 0)
            {
                unsigned uart = fd->pty.uart;
                fd->type = LINUX_FD_TYPE_SOCKET_SERVER;
                fd->socket_server.uart = uart;
                break;
            }
        }
    }

    fprintf(stdout, "-------------\n");
    fprintf(stdout, "Process ID: %"PRIi32"\n", getpid());
    signal(SIGINT, _linux_sig_handler);
    signal(SIGUSR1, _linux_sig_handler);
    signal(SIGSEGV, _linux_sig_handler);
    signal(SIGTERM, _linux_sig_handler);
    signal(SIGFPE, _linux_sig_handler);
    signal(SIGQUIT, _linux_sig_handler);
    signal(SIGBUS, _linux_sig_handler);
    signal(SIGILL, _linux_sig_handler);
    _linux_setup_fd_handlers();
    _linux_setup_poll();
    pthread_create(&_linux_listener_thread_id, NULL, thread_proc, NULL);

    model_linux_spawn_fakes();
}


void platform_start(void)
{
    char * overloaded_log_debug_mask = getenv("DEBUG_MASK");
    if (overloaded_log_debug_mask)
    {
        linux_port_debug("New debug mask: %s", overloaded_log_debug_mask);
        log_debug_mask = DEBUG_SYS | strtoul(overloaded_log_debug_mask, NULL, 16);
        persist_set_log_debug_mask(log_debug_mask);
        log_debug_mask = log_debug_mask;
    }
    char * meas_interval = getenv("MEAS_INTERVAL");
    if (meas_interval)
    {
        unsigned mins = strtoul(meas_interval, NULL, 10);
        linux_port_debug("New Measurement Interval: %u", mins);
        persist_data.model_config.mins_interval = mins * 1000;
        transmit_interval = mins;
    }
    char * auto_meas = getenv("AUTO_MEAS");
    if (auto_meas)
    {
        unsigned auto_meas_int = strtoul(auto_meas, NULL, 10);
        measurements_enabled = (auto_meas_int > 0);
        linux_port_debug("Auto Measurements: %u", auto_meas_int);
    }
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


void platform_set_rs485_mode(bool driver_enable)
{
}


void platform_reset_sys(void)
{
    _linux_save_fd_file();

    fprintf(stdout, "Collecting threads...\n");
    linux_threads_deinit = true;
    pthread_join(_linux_listener_thread_id, NULL);
    fprintf(stdout, "Cleaning up before exit...\n");
    i2c_linux_deinit();
    w1_linux_deinit();
    model_linux_close_fakes();

    char link_addr[128];
    char proc_ln[64];
    snprintf(proc_ln, 63, "/proc/%"PRIi32"/exe", getpid());
    ssize_t len = readlink(proc_ln, link_addr, 127);
    link_addr[len] = 0;
    char *const __argv[1] = {0};
    if (execv(link_addr, __argv))
        linux_error("Could not re-exec firmware");
}

char concat_osm_location(char* new_loc, int loc_len, char* global)
{
    unsigned len = snprintf(new_loc, loc_len - 1, "%s%s", ret_static_file_location(), global);
    new_loc[len] = '\0';
    return len;
}

static persist_mem_t* _linux_get_persist(void)
{
    char osm_img_loc[LOCATION_LEN];
    concat_osm_location(osm_img_loc, LOCATION_LEN, LINUX_PERSIST_FILE_LOC);
    FILE* mem_file = fopen(osm_img_loc, "rb");
    if (!mem_file)
        return NULL;
    if (fread(&_linux_persist_mem, sizeof(persist_mem_t), 1, mem_file) != 1)
    {
        fclose(mem_file);
        return NULL;
    }
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
    char osm_img_loc[LOCATION_LEN];
    concat_osm_location(osm_img_loc, LOCATION_LEN, LINUX_PERSIST_FILE_LOC);
    FILE* mem_file = fopen(osm_img_loc, "wb");
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
#ifdef HPM_UART
{
    const char* on_data = "ON\n";
    const char* off_data = "OFF\n";
    if (enable)
    {
        uart_blocking(HPM_UART, on_data, strlen(on_data));
        return;
    }
    uart_blocking(HPM_UART, off_data, strlen(off_data));
}
#else //HPM_UART
{}
#endif //HPM_UART


void platform_tight_loop(void)
{
    static int64_t last_call = 0;
    int64_t now = linux_get_current_us();
    if (!last_call)
    {
        last_call = now;
        return;
    }
    int64_t delta_time = (now - last_call);
    if (delta_time < 1000)
        linux_usleep(1000 - delta_time);
    last_call = now;
}


void __attribute__((weak)) model_main_loop_iterate(void) {}


void platform_main_loop_iterate(void)
{
    model_main_loop_iterate();
}


uint32_t get_since_boot_ms(void)
{
    int64_t t = (linux_get_current_us() - _linux_boot_time_us) / 1000;
    return t;
}


void linux_usleep(unsigned usecs)
{
    if (!_linux_running)
        return; /* No sleep for the dying. */
    int64_t end_time = linux_get_current_us() + usecs;

    struct timespec ts = {.tv_sec = end_time / 1000000,
                          .tv_nsec = (end_time % 1000000) * 1000};

    if (!pthread_mutex_lock(&_sleep_mutex))
    {
        int rc = pthread_cond_timedwait(&_sleep_cond, &_sleep_mutex, &ts);
        if (!rc || rc == ETIMEDOUT)
            pthread_mutex_unlock(&_sleep_mutex);
    }
    else printf("FAILED to sleep.\n");
}


void linux_awaken(void)
{
    if (!pthread_mutex_lock(&_sleep_mutex))
    {
        pthread_cond_broadcast(&_sleep_cond);
        pthread_mutex_unlock(&_sleep_mutex);
    }
    else printf("Sleep Linux not kicked.\n");
}


void platform_gpio_init(const port_n_pins_t * gpio_pin)
{
}


void platform_gpio_setup(const port_n_pins_t * gpio_pin, bool is_input, uint32_t pull)
{
}


void platform_gpio_set(const port_n_pins_t * gpio_pin, bool is_on)
{
    _ios_enabled[gpio_pin->index] = is_on;
}


bool platform_gpio_get(const port_n_pins_t * gpio_pin)
{
    return _ios_enabled[gpio_pin->index];
}


bool socket_connect(char* path, int* _socketfd)
{
    if (!path || !_socketfd)
        return false;
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    *_socketfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 5000;
    setsockopt(*_socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    if (connect(*_socketfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        printf("Could not bind the socket : %s\n", path);
        return false;
    }
    return true;
}
