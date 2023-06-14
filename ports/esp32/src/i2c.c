#include <driver/i2c.h>

#include "i2c.h"

#include "pinmap.h"
#include "log.h"
#include "common.h"
#include "uart_rings.h"


static const i2c_def_t i2c_buses[]     = I2C_BUSES;
static uint8_t         i2c_buses_ready = 0;


static void i2c_init(unsigned i2c_index)
{
/*    if (i2c_index > ARRAY_SIZE(i2c_buses))
    {
        log_error("Tried to init I2C bus with uninitialised memory.");
        return;
    }

    if (!log_async_log && (i2c_buses_ready & (1 << i2c_index)))
        return;

    i2c_buses_ready |= (1 << i2c_index);

    const i2c_def_t * i2c_bus = &i2c_buses[i2c_index];

    esp_err_t err = i2c_param_config(i2c_bus->port, &i2c_bus->config);

    if (err == ESP_OK)
    {
        log_error("Failed to configure i2c : %s", esp_err_to_name(err));
        return;
    }

    err = i2c_driver_install(i2c_bus->port, i2c_bus->config.mode, 0, 0, 0);

    if (err == ESP_OK)
    {
        log_error("Failed setup i2c driver : %s", esp_err_to_name(err));
        return;
    }*/
}


bool i2c_transfer_timeout(uint32_t i2c, uint8_t addr, const uint8_t *w, unsigned wn, uint8_t *r, unsigned rn, unsigned timeout_ms)
{
    const i2c_def_t * i2c_bus = &i2c_buses[i2c];
    esp_err_t err = ESP_OK;

    if (wn)
    {
        if (rn)
            err = i2c_master_write_read_device(i2c_bus->port, addr, (uint8_t*)w, wn, r, rn, timeout_ms / portTICK_PERIOD_MS);
        else
            err = i2c_master_write_to_device(i2c_bus->port, addr, (uint8_t*)w, wn, timeout_ms / portTICK_PERIOD_MS);
    } if (rn)
        err = i2c_master_read_from_device(i2c_bus->port, addr, r, rn, timeout_ms / portTICK_PERIOD_MS);

    if (err == ESP_OK)
    {
        log_error("Failed i2c transfer : %s", esp_err_to_name(err));
        return false;
    }

    return true;
}


static void i2c_deinit(unsigned i2c_index)
{
    if (i2c_index > ARRAY_SIZE(i2c_buses))
    {
        log_error("Tried to deinit I2C bus with uninitialised memory.");
        return;
    }

    if (!log_async_log && !(i2c_buses_ready & (1 << i2c_index)))
        return;

    i2c_buses_ready &= ~(1 << i2c_index);

    const i2c_def_t * i2c_bus = &i2c_buses[i2c_index];

    esp_err_t err = i2c_driver_delete(i2c_bus->port);

    if (err == ESP_OK)
        log_error("Failed i2c deinit : %s", esp_err_to_name(err));
}


void i2cs_init(void)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(i2c_buses); i++)
        i2c_init(i);
}


void i2cs_deinit(void)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(i2c_buses); i++)
        i2c_deinit(i);
}
