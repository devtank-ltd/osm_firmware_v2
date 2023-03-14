#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

#include "linux.h"
#include "peripherals.h"

#define FAKE_I2C_SERVER     "peripherals/i2c_server.py"
#define FAKE_W1_SERVER      "peripherals/w1_server.py"
#define FAKE_HPM_SERVER     "peripherals/hpm_virtual.py"
#define FAKE_MODBUS_SERVER  "peripherals/modbus_server.py"


#define FAKE_HPM_TTY           "UART_HPM"
#define FAKE_MODBUS_TTY        "UART_EXT"

#define FAKE_I2C_SOCKET        "i2c_socket"
#define FAKE_1W_SOCKET         "w1_socket"


void peripherals_add_modbus(unsigned uart, unsigned* pid)
{
    peripherals_add_uart_tty_bridge(FAKE_MODBUS_TTY, uart);
    *pid = linux_spawn(FAKE_MODBUS_SERVER);
}


void peripherals_add_hpm(unsigned uart, unsigned* pid)
{
    peripherals_add_uart_tty_bridge(FAKE_HPM_TTY, uart);
    *pid = linux_spawn(FAKE_HPM_SERVER);
}


void peripherals_add_w1(unsigned timeout_us, unsigned* pid)
{
    char w1_socket[LOCATION_LEN];
    concat_osm_location(w1_socket, LOCATION_LEN, FAKE_1W_SOCKET);
    peripherals_add(FAKE_W1_SERVER, w1_socket, timeout_us, pid);
}


void peripherals_add_i2c(unsigned timeout_us, unsigned* pid)
{
    char i2c_socket[LOCATION_LEN];
    concat_osm_location(i2c_socket, LOCATION_LEN, FAKE_I2C_SOCKET);
    peripherals_add(FAKE_I2C_SERVER, i2c_socket, timeout_us, pid);
}


bool peripherals_add(const char * app_rel_path, const char * ready_path, unsigned timeout_us, unsigned* pid)
{
    unlink(ready_path);
    *pid = linux_spawn(app_rel_path);
    char pid_path[PATH_MAX];

    snprintf(pid_path, PATH_MAX, "/proc/%u", *pid);

    int64_t start_time = linux_get_current_us();

    while(linux_get_current_us() < (start_time + timeout_us))
    {
        struct stat buf;
        if (stat(pid_path, &buf) != 0)
        {
            linux_error("While, waiting for %s, PID:%u closed.", ready_path, *pid);
            return false;
        }
        if (stat(ready_path, &buf) == 0)
            return true;
        usleep(1000);
    }
    linux_error("Wait of %u for %s failed", timeout_us, ready_path);
    return false;
}

