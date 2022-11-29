#include "linux.h"

#define FAKE_I2C_SERVER     "peripherals/i2c_server.py"
#define FAKE_W1_SERVER      "peripherals/w1_server.py"
#define FAKE_HPM_SERVER     "peripherals/hpm_virtual.py"
#define FAKE_MODBUS_SERVER  "peripherals/modbus_server.py"


#define FAKE_HPM_TTY           "UART_HPM"
#define FAKE_MODBUS_TTY        "UART_EXT"



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


void peripherals_add_w1(void)
{
    linux_spawn(FAKE_W1_SERVER);
}


void peripherals_add_i2c(void)
{
    linux_spawn(FAKE_I2C_SERVER);
}

