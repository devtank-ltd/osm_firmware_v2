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

    // It is important to have as small a gap as possible between Port bring up and pins being taken over by I2C, or it can be taken as a start bit!
    RCC_CCIPR |= (RCC_CCIPR_I2CxSEL_APB << RCC_CCIPR_I2C1SEL_SHIFT);
    rcc_periph_clock_enable(i2c_bus->rcc);
    rcc_periph_clock_enable(PORT_TO_RCC(i2c_bus->port_n_pins.port));
    gpio_set_af(i2c_bus->port_n_pins.port, i2c_bus->gpio_func, i2c_bus->port_n_pins.pins);
    gpio_mode_setup(i2c_bus->port_n_pins.port,  GPIO_MODE_AF, GPIO_PUPD_NONE, i2c_bus->port_n_pins.pins);
    gpio_set_output_options(i2c_bus->port_n_pins.port, GPIO_OTYPE_OD, GPIO_OSPEED_VERYHIGH, i2c_bus->port_n_pins.pins);

    i2c_reset(i2c_bus->i2c);
    i2c_peripheral_disable(i2c_bus->i2c);

    i2c_set_scl_low_period(i2c_bus->i2c, 236);
    i2c_set_scl_high_period(i2c_bus->i2c, 156);
    i2c_set_data_hold_time(i2c_bus->i2c, 0);
    i2c_set_data_setup_time(i2c_bus->i2c, 9);
    i2c_set_prescaler(i2c_bus->i2c, 1);

    i2c_set_7bit_addr_mode(i2c_bus->i2c);
    i2c_enable_analog_filter(i2c_bus->i2c);
    i2c_set_digital_filter(i2c_bus->i2c, 0);

    i2c_peripheral_enable(i2c_bus->i2c);
}
