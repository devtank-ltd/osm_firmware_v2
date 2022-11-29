#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

#include "linux.h"

#define FAKE_I2C_SERVER     "peripherals/i2c_server.py"
#define FAKE_W1_SERVER      "peripherals/w1_server.py"
#define FAKE_HPM_SERVER     "peripherals/hpm_virtual.py"
#define FAKE_MODBUS_SERVER  "peripherals/modbus_server.py"


#define FAKE_HPM_TTY           "UART_HPM"
#define FAKE_MODBUS_TTY        "UART_EXT"

#define FAKE_I2C_SOCKET        LINUX_FILE_LOC"i2c_socket"
#define FAKE_1W_SOCKET         LINUX_FILE_LOC"w1_socket"



void peripherals_add_modbus(unsigned uart)
{
    peripherals_add_uart_tty_bridge(FAKE_MODBUS_TTY, uart);
    linux_spawn(FAKE_MODBUS_SERVER);
}

void peripherals_add_hpm(unsigned uart)
{
    peripherals_add_uart_tty_bridge(FAKE_HPM_TTY, uart);
    linux_spawn(FAKE_HPM_SERVER);
}


static bool _wait_for_file(unsigned pid, const char * path, unsigned timeout_us)
{
    char pid_path[PATH_MAX];

    snprintf(pid_path, PATH_MAX, "/proc/%u", pid);

    int64_t start_time = linux_get_current_us();

    while(linux_get_current_us() < (start_time + timeout_us))
    {
        struct stat buf;
        if (stat(pid_path, &buf) != 0)
        {
            linux_error("While, waiting for %s, PID:%u closed.", path, pid);
            return false;
        }
        if (stat(path, &buf) == 0)
            return true;
        usleep(1000);
    }
    linux_error("Wait of %u for %s failed", timeout_us, path);
    return false;
}


void peripherals_add_w1(unsigned timeout_us)
{
    unlink(FAKE_1W_SOCKET);
    unsigned pid = linux_spawn(FAKE_W1_SERVER);
    if (_wait_for_file(pid, FAKE_1W_SOCKET, timeout_us))
        linux_port_debug("w1 Ready");
}


void peripherals_add_i2c(unsigned timeout_us)
{
    unlink(FAKE_I2C_SOCKET);
    unsigned pid = linux_spawn(FAKE_I2C_SERVER);
    if (!_wait_for_file(pid, FAKE_I2C_SOCKET, timeout_us))
        linux_port_debug("i2c Ready");
}

