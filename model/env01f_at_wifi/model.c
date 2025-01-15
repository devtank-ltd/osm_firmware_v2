#include <string.h>

#include <osm/core/measurements.h>

#include <libopencm3/stm32/gpio.h>

#include <osm/core/timers.h>
#include <osm/core/io.h>
#include <osm/core/adcs.h>
#include <osm/sensors/can_impl.h>
#include <osm/core/log.h>
#include <osm/core/config.h>
#include "pinmap.h"
#include <osm/core/uart_rings.h>
#include <osm/sensors/senxx.h>
#include <osm/sensors/cc.h>
#include <osm/core/modbus_measurements.h>
#include <osm/sensors/ds18b20.h>
#include <osm/sensors/pulsecount.h>
#include <osm/sensors/veml7700.h>
#include <osm/sensors/sai.h>
#include <osm/sensors/fw.h>
#include <osm/core/persist_config.h>
#include <osm/core/sleep.h>
#include <osm/core/update.h>
#include <osm/core/modbus.h>
#include "model.h"
#include <osm/core/platform.h>
#include <osm/core/w1.h>
#include <osm/sensors/io_watch.h>
#include "model_config.h"
#include "comms_direct.h"
#include <osm/sensors/example_rs232.h>
#include <osm/comms/comms_identify.h>


uint8_t osm_model_stm_adcs_get_channel(osm_adcs_type_t adcs_type)
{
    switch(adcs_type)
    {
        case OSM_ADCS_TYPE_CC_CLAMP1: return ADC1_CHANNEL_CURRENT_CLAMP_1;
        case OSM_ADCS_TYPE_CC_CLAMP2: return ADC1_CHANNEL_CURRENT_CLAMP_2;
        case OSM_ADCS_TYPE_CC_CLAMP3: return ADC1_CHANNEL_CURRENT_CLAMP_3;
        default:
            break;
    }
    return 0;
}


void osm_model_persist_config_model_init(osm_persist_model_config_t* model_config)
{
    model_config->mins_interval = 5 * 1000 / 60; /* 5 seconds */
    osm_cc_setup_default_mem(model_config->cc_configs, sizeof(model_config->cc_configs));
    osm_at_wifi_config_init(&model_config->comms_config);
    model_config->sai_no_buf = OSM_SAI_DEFAULT_NO_BUF;
    for (unsigned i = 0; i < W1_PULSE_COUNT; i++)
    {
        model_config->pulsecount_debounces_ms[i] = 50;
    }
}


/* Return true  if different
 *        false if same      */
bool osm_model_persist_config_cmp(osm_persist_model_config_t* d0, osm_persist_model_config_t* d1)
{
    return !(
        d0 && d1 &&
        d0->mins_interval == d1->mins_interval &&
        !osm_modbus_persist_config_cmp(&d0->modbus_bus, &d1->modbus_bus) &&
        !osm_comms_persist_config_cmp(&d0->comms_config, &d1->comms_config) &&
        memcmp(d0->cc_configs, d1->cc_configs, sizeof(osm_cc_config_t) * ADC_CC_COUNT) == 0 &&
        memcmp(d0->ios_state, d1->ios_state, sizeof(uint16_t) * IOS_COUNT) == 0 &&
        memcmp(d0->sai_cal_coeffs, d1->sai_cal_coeffs, sizeof(float) * OSM_SAI_NUM_CAL_COEFFS) == 0 &&
        d0->sai_no_buf == d1->sai_no_buf &&
        memcmp(d0->pulsecount_debounces_ms, d1->pulsecount_debounces_ms, sizeof(uint32_t) * W1_PULSE_COUNT) == 0);
}


static void _model_core_3v3_init(void)
{
}


void osm_model_sensors_init(void)
{
    _model_core_3v3_init();
    osm_timers_init();
    osm_pulsecount_init();
    osm_ios_init();
    osm_sai_init();
    osm_adcs_init();
    osm_cc_init();
    osm_veml7700_init();
    osm_ds18b20_temp_init();
    osm_modbus_init();
    osm_can_impl_init();
    osm_senxx_init();
    osm_comms_direct_init();
}


void osm_model_post_init(void)
{
    osm_io_watch_init();
}


bool osm_model_uart_ring_done_in_process(unsigned uart, osm_ring_buf_t * ring)
{
    if (uart == EXT_UART)
    {
        osm_modbus_uart_ring_in_process(ring);
        return true;
    }
    else if (uart == RS232_UART)
    {
        osm_example_rs232_process(ring);
        return true;
    }

    return false;
}


bool osm_model_uart_ring_do_out_drain(unsigned uart, osm_ring_buf_t * ring)
{
    if (uart == EXT_UART)
        return osm_modbus_uart_ring_do_out_drain(ring);
    return true;
}


bool osm_model_measurements_get_inf(osm_measurements_def_t * def, osm_measurements_data_t* data, osm_measurements_inf_t* inf)
{
    if (!def || !inf)
    {
        osm_measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(osm_measurements_inf_t));
    switch(def->type)
    {
        case OSM_FW_VERSION:    osm_fw_version_inf_init(inf);  break;
        case OSM_CONFIG_REVISION: osm_persist_config_inf_init(inf);  break;
        case OSM_MODBUS:        osm_modbus_inf_init(inf);      break;
        case OSM_CURRENT_CLAMP: osm_cc_inf_init(inf);          break;
        case OSM_W1_PROBE:      osm_ds18b20_inf_init(inf);     break;
        case OSM_PULSE_COUNT:   osm_pulsecount_inf_init(inf);  break;
        case OSM_LIGHT:         osm_veml7700_inf_init(inf);    break;
        case OSM_SOUND:         osm_sai_inf_init(inf);         break;
        case OSM_IO_READING:    osm_ios_inf_init(inf);         break;
        case OSM_SENxx:         osm_senxx_inf_init(inf);       break;
        case OSM_EXAMPLE_RS232:         osm_example_rs232_inf_init(inf);       break;
        default:
            osm_log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


void osm_model_measurements_repopulate(void)
{
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_FW_VERSION,         100,  1,  OSM_FW_VERSION      );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_CONFIG_REVISION,    100,  1,  OSM_CONFIG_REVISION );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_PM1_0_NAME,           0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_PM25_NAME,            0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_PM4_NAME,             0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_PM10_NAME,            0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_REL_HUM_NAME,         0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_SEN5x_TEMP_NAME,      0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_VOC_NAME,             0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_NOX_NAME,             0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_CO2_NAME,             0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_HCHO_NAME,            0,  5,  OSM_SENxx           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  1,  OSM_CURRENT_CLAMP   );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  1,  OSM_CURRENT_CLAMP   );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  1,  OSM_CURRENT_CLAMP   );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_W1_PROBE_NAME_1,      0,  1,  OSM_W1_PROBE        );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  OSM_PULSE_COUNT     );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  OSM_PULSE_COUNT     );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_LIGHT_NAME,           1,  1,  OSM_LIGHT           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_SOUND_NAME,           1,  1,  OSM_SOUND           );
    osm_measurements_repop_indiv(OSM_MEASUREMENTS_EXAMPLE_RS232_NAME,   0,  1,  OSM_EXAMPLE_RS232   );
}


void osm_model_cmds_add_all(struct osm_cmd_link_t* tail)
{
    tail = osm_cc_add_commands(tail);
    tail = osm_can_impl_add_commands(tail);
    tail = osm_sai_add_commands(tail);
    tail = osm_persist_config_add_commands(tail);
    tail = osm_measurements_add_commands(tail);
    tail = osm_ios_add_commands(tail);
    tail = osm_modbus_add_commands(tail);
    tail = osm_sleep_add_commands(tail);
    tail = osm_update_add_commands(tail);
    tail = osm_comms_add_commands(tail);
    tail = osm_senxx_add_commands(tail);
    tail = osm_comms_direct_add_commands(tail);
    tail = osm_comms_identify_add_commands(tail);
}


void osm_model_w1_pulse_enable_pupd(unsigned io, bool enabled)
{
}


bool osm_model_can_io_be_special(unsigned io, osm_io_special_t special)
{
    return ((      io == W1_PULSE_1_IO                      ||      io == W1_PULSE_2_IO                         ) &&
            ( special == OSM_IO_SPECIAL_ONEWIRE                 || special == OSM_IO_SPECIAL_PULSECOUNT_RISING_EDGE ||
              special == OSM_IO_SPECIAL_PULSECOUNT_FALLING_EDGE || special == OSM_IO_SPECIAL_PULSECOUNT_BOTH_EDGE   ||
              special == OSM_IO_SPECIAL_WATCH                   ));
}


void osm_model_uarts_setup(void)
{
    uint32_t reg32 = RCC_CCIPR & ~(RCC_CCIPR_LPUART1SEL_MASK << RCC_CCIPR_LPUART1SEL_SHIFT);
    RCC_CCIPR = reg32 | ((RCC_CCIPR_LPUART1SEL_HSI16 & RCC_CCIPR_LPUART1SEL_MASK) << RCC_CCIPR_LPUART1SEL_SHIFT);
}


void osm_model_setup_pulse_pupd(uint8_t* pupd)
{
}


unsigned osm_model_measurements_add_defaults(osm_measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,         100,  1, FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CONFIG_REVISION,    100,  1, CONFIG_REVISION );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM1_0_NAME,           0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM25_NAME,            0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM4_NAME,             0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM10_NAME,            0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_REL_HUM_NAME,         0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SEN5x_TEMP_NAME,      0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_VOC_NAME,             0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_NOX_NAME,             0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CO2_NAME,             0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HCHO_NAME,            0,  5, SENxx           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  1, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  1, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  1, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_W1_PROBE_NAME_1,      0,  1, W1_PROBE        );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1, PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1, PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_LIGHT_NAME,           1,  1, LIGHT           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SOUND_NAME,           1,  1, SOUND           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_EXAMPLE_RS232_NAME,           0,  1, EXAMPLE_RS232           );
    ios_measurements_init();
    return pos;
}


void osm_model_main_loop_iterate(void)
{
    osm_comms_direct_loop_iterate();
    osm_senxx_iterate();
}


bool __attribute__((weak)) osm_bat_on_battery(bool* on_battery)
{
    /* No battery fitted */
    return false;
}
