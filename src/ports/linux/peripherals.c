#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include <signal.h>
#include <sys/wait.h>

#include "linux.h"
#include "peripherals.h"

#define FAKE_I2C_SERVER     "peripherals/i2c_server.py"
#define FAKE_W1_SERVER      "peripherals/w1_server.py"
#define FAKE_HPM_SERVER     "peripherals/hpm_virtual.py"
#define FAKE_MODBUS_SERVER  "peripherals/modbus_server.py"
#define FAKE_CMD_SERVER     "peripherals/command_server.py"
#define FAKE_EXAMPLE_RS232_SERVER   "peripherals/example_rs232.py"


#define FAKE_HPM_TTY           "UART_HPM"
#define FAKE_MODBUS_TTY        "UART_EXT"
#define FAKE_CMD_TTY           "UART_CMD"
#define FAKE_EXAMPLE_RS232_TTY         "UART_EXAMPLE_RS232"

#define FAKE_I2C_SOCKET        "i2c_socket"
#define FAKE_1W_SOCKET         "w1_socket"


void osm_peripherals_add_modbus(unsigned uart, unsigned* pid)
{
    osm_peripherals_add_uart_tty_bridge(FAKE_MODBUS_TTY, uart);
    *pid = osm_linux_spawn(FAKE_MODBUS_SERVER);
}


void osm_peripherals_add_hpm(unsigned uart, unsigned* pid)
{
    osm_peripherals_add_uart_tty_bridge(FAKE_HPM_TTY, uart);
    *pid = osm_linux_spawn(FAKE_HPM_SERVER);
}

void osm_peripherals_add_cmd(unsigned* pid)
{
    *pid = osm_linux_spawn(FAKE_CMD_SERVER);
}

void osm_peripherals_add_w1(unsigned timeout_us, unsigned* pid)
{
    char w1_socket[OSM_LOCATION_LEN];
    osm_concat_osm_location(w1_socket, OSM_LOCATION_LEN, FAKE_1W_SOCKET);
    osm_peripherals_add(FAKE_W1_SERVER, w1_socket, timeout_us, pid);
}


void osm_peripherals_add_i2c(unsigned timeout_us, unsigned* pid)
{
    char i2c_socket[OSM_LOCATION_LEN];
    osm_concat_osm_location(i2c_socket, OSM_LOCATION_LEN, FAKE_I2C_SOCKET);
    osm_peripherals_add(FAKE_I2C_SERVER, i2c_socket, timeout_us, pid);
}


void osm_peripherals_add_example_rs232(unsigned uart, unsigned* pid)
{
    osm_peripherals_add_uart_tty_bridge(FAKE_EXAMPLE_RS232_TTY, uart);
    *pid = osm_linux_spawn(FAKE_EXAMPLE_RS232_SERVER);
}


bool osm_peripherals_add(const char * app_rel_path, const char * ready_path, unsigned timeout_us, unsigned* pid)
{
    unlink(ready_path);
    *pid = osm_linux_spawn(app_rel_path);
    char pid_path[PATH_MAX];

    snprintf(pid_path, PATH_MAX, "/proc/%u", *pid);

    int64_t start_time = osm_linux_get_current_us();

    while(osm_linux_get_current_us() < (start_time + timeout_us))
    {
        struct stat buf;
        if (stat(pid_path, &buf) != 0)
        {
            osm_linux_error("While, waiting for %s, PID:%u closed.", ready_path, *pid);
            return false;
        }
        if (stat(ready_path, &buf) == 0)
            return true;
        usleep(1000);
    }
    osm_linux_error("Wait of %u for %s failed", timeout_us, ready_path);
    return false;
}


void osm_peripherals_close(unsigned* pids, unsigned count)
{
    for(unsigned n=0; n<count; n++)
    {
        unsigned pid = pids[n];
        if (pid)
        {
            osm_linux_port_debug("Killing child PID %u", pid);
            kill(pid, SIGINT);
            waitpid(pid, NULL, 0);
        }
    }
}


