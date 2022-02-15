#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>

#include "pinmap.h"
#include "log.h"
#include "common.h"


static const i2c_def_t i2c_buses[]     = I2C_BUSES;
static uint8_t         i2c_buses_ready = 0;


void    i2c_init(unsigned i2c_index)
{
    if (!log_async_log && (i2c_buses_ready & (1 << i2c_index)))
        return;

    i2c_buses_ready |= (1 << i2c_index);

    const i2c_def_t * i2c_bus = &i2c_buses[i2c_index];

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


bool i2c_transfer_timeout(uint32_t i2c, uint8_t addr, const uint8_t *w, unsigned wn, uint8_t *r, unsigned rn, unsigned timeout_ms)
{
    /* i2c_transfer7 but with ms timeout. */
    uint32_t start_ms = get_since_boot_ms();

    if (wn)
    {
        i2c_set_7bit_address(i2c, addr);
        i2c_set_write_transfer_dir(i2c);
        i2c_set_bytes_to_transfer(i2c, wn);
        if (rn)
            i2c_disable_autoend(i2c);
        else
            i2c_enable_autoend(i2c);

        i2c_send_start(i2c);

        while (wn--)
        {
            bool wait = true;
            while (wait)
            {
                if (i2c_transmit_int_status(i2c))
                {
                    wait = false;
                }
                while (i2c_nack(i2c))
                {
                    if (since_boot_delta(get_since_boot_ms(), start_ms) > timeout_ms)
                    {
                        log_error("I2C timeout");
                        return false;
                    }
                }
            }
            i2c_send_data(i2c, *w++);
        }

        if (rn)
        {
            while (!i2c_transfer_complete(i2c))
            {
                if (since_boot_delta(get_since_boot_ms(), start_ms) > timeout_ms)
                {
                    log_error("I2C timeout");
                    return false;
                }
            }
        }
    }

    if (rn)
    {
        i2c_set_7bit_address(i2c, addr);
        i2c_set_read_transfer_dir(i2c);
        i2c_set_bytes_to_transfer(i2c, rn);
        i2c_send_start(i2c);

        i2c_enable_autoend(i2c);

        for (size_t i = 0; i < rn; i++)
        {
            while (i2c_received_data(i2c) == 0)
            {
                if (since_boot_delta(get_since_boot_ms(), start_ms) > timeout_ms)
                {
                    log_error("I2C timeout");
                    return false;
                }
            }
            r[i] = i2c_get_data(i2c);
        }
    }

    return true;
}
