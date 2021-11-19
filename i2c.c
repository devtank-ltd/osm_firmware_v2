#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>

#include "pinmap.h"


static const i2c_def_t i2c_buses[]     = I2C_BUSES;
static uint8_t         i2c_buses_ready = 0;


void    i2c_init(unsigned i2c_index)
{
    if (i2c_buses_ready & (1 << i2c_index))
        return;

    i2c_buses_ready |= (1 << i2c_index);

    const i2c_def_t * i2c_bus = &i2c_buses[i2c_index];

    for(unsigned n = 0; n < 2; n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(i2c_bus->port_n_pins[n].port));

        gpio_mode_setup(i2c_bus->port_n_pins[n].port,
                        GPIO_MODE_AF,
                        GPIO_PUPD_NONE,
                        i2c_bus->port_n_pins[n].pins);

        gpio_set_af(i2c_bus->port_n_pins[n].port,
                    i2c_bus->funcs[n],
                    i2c_bus->port_n_pins[n].pins);
    }

    rcc_periph_clock_enable(i2c_bus->rcc);

    i2c_reset(i2c_bus->i2c);
    i2c_peripheral_disable(i2c_bus->i2c);
    i2c_set_speed(i2c_bus->i2c, i2c_bus->speed, i2c_bus->clock_megahz);
    i2c_set_7bit_addr_mode(i2c_bus->i2c);
    i2c_peripheral_enable(i2c_bus->i2c);
}
