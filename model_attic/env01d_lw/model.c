#include <string.h>

#include "measurements.h"

#include <libopencm3/stm32/gpio.h>

#include "timers.h"
#include "io.h"
#include "adcs.h"
#include "can_impl.h"
#include "log.h"
#include "config.h"
#include "pinmap.h"
#include "uart_rings.h"
#include "sen54.h"
#include "cc.h"
#include "modbus_measurements.h"
#include "ds18b20.h"
#include "pulsecount.h"
#include "veml7700.h"
#include "sai.h"
#include "fw.h"
#include "persist_config.h"
#include "sleep.h"
#include "update.h"
#include "modbus.h"
#include "model.h"
#include "platform.h"
#include "w1.h"
#include "io_watch.h"
#include "model_config.h"
#include "lw.h"


uint8_t model_stm_adcs_get_channel(adcs_type_t adcs_type)
{
    switch(adcs_type)
    {
        case ADCS_TYPE_CC_CLAMP1: return ADC1_CHANNEL_CURRENT_CLAMP_1;
        case ADCS_TYPE_CC_CLAMP2: return ADC1_CHANNEL_CURRENT_CLAMP_2;
        case ADCS_TYPE_CC_CLAMP3: return ADC1_CHANNEL_CURRENT_CLAMP_3;
        default:
            break;
    }
    return 0;
}


void model_persist_config_model_init(persist_model_config_v1_t* model_config)
{
    model_config->mins_interval = 5 * 1000 / 60; /* 5 seconds */
    cc_setup_default_mem(model_config->cc_configs, sizeof(model_config->cc_configs));
    lw_config_init(&model_config->comms_config);
    model_config->sai_no_buf = SAI_DEFAULT_NO_BUF;
}


/* Return true  if different
 *        false if same      */
bool model_persist_config_cmp(persist_model_config_v1_t* d0, persist_model_config_v1_t* d1)
{
    return !(
        d0 && d1 &&
        d0->mins_interval == d1->mins_interval &&
        !modbus_persist_config_cmp(&d0->modbus_bus, &d1->modbus_bus) &&
        !comms_persist_config_cmp(&d0->comms_config, &d1->comms_config) &&
        memcmp(d0->cc_configs, d1->cc_configs, sizeof(cc_config_t) * ADC_CC_COUNT) == 0 &&
        memcmp(d0->ios_state, d1->ios_state, sizeof(uint16_t) * IOS_COUNT) == 0 &&
        memcmp(d0->sai_cal_coeffs, d1->sai_cal_coeffs, sizeof(float) * SAI_NUM_CAL_COEFFS) == 0 &&
        d0->sai_no_buf == d1->sai_no_buf );
}


static void _model_core_3v3_init(void)
{
}


void model_sensors_init(void)
{
    _model_core_3v3_init();
    timers_init();
    ios_init();
    sai_init();
    adcs_init();
    cc_init();
    veml7700_init();
    ds18b20_temp_init();
    pulsecount_init();
    modbus_init();
    can_impl_init();
    sen54_init();
}


void model_post_init(void)
{
    io_watch_init();
}


bool model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
    {
        modbus_uart_ring_in_process(ring);
        return true;
    }
    /* If RS232 is in use, take the `process` from here
    else if (uart == HPM_UART)
    {
        hpm_ring_process(ring, line_buffer, CMD_LINELEN);
        return true;
    }
    */

    return false;
}


bool model_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
        return modbus_uart_ring_do_out_drain(ring);
    return true;
}


bool model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !inf)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(measurements_inf_t));
    switch(def->type)
    {
        case FW_VERSION:    fw_version_inf_init(inf);  break;
        case CONFIG_REVISION: persist_config_inf_init(inf);  break;
        case MODBUS:        modbus_inf_init(inf);      break;
        case CURRENT_CLAMP: cc_inf_init(inf);          break;
        case W1_PROBE:      ds18b20_inf_init(inf);     break;
        case PULSE_COUNT:   pulsecount_inf_init(inf);  break;
        case LIGHT:         veml7700_inf_init(inf);    break;
        case SOUND:         sai_inf_init(inf);         break;
        case IO_READING:    ios_inf_init(inf);         break;
        case SEN54:         sen54_inf_init(inf);       break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


void model_measurements_repopulate(void)
{
    measurements_repop_indiv(MEASUREMENTS_FW_VERSION,         100,  1,  FW_VERSION      );
    measurements_repop_indiv(MEASUREMENTS_CONFIG_REVISION,    100,  1,  CONFIG_REVISION );
    measurements_repop_indiv(MEASUREMENTS_PM1_0_NAME,           0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_PM25_NAME,            0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_PM4_NAME,             0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_PM10_NAME,            0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_REL_HUM_NAME,         0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_SEN54_TEMP_NAME,      0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_VOC_NAME,             0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_NOX_NAME,             0,  5,  SEN54           );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  1,  CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  1,  CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  1,  CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_W1_PROBE_NAME_1,      0,  1,  W1_PROBE        );
    measurements_repop_indiv(MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_repop_indiv(MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_repop_indiv(MEASUREMENTS_LIGHT_NAME,           1,  1,  LIGHT           );
    measurements_repop_indiv(MEASUREMENTS_SOUND_NAME,           1,  1,  SOUND           );
}


void model_cmds_add_all(struct cmd_link_t* tail)
{
    tail = cc_add_commands(tail);
    tail = can_impl_add_commands(tail);
    tail = sai_add_commands(tail);
    tail = persist_config_add_commands(tail);
    tail = measurements_add_commands(tail);
    tail = ios_add_commands(tail);
    tail = modbus_add_commands(tail);
    tail = sleep_add_commands(tail);
    tail = update_add_commands(tail);
    tail = comms_add_commands(tail);
    tail = sen54_add_commands(tail);
}


void model_w1_pulse_enable_pupd(unsigned io, bool enabled)
{
}


bool model_can_io_be_special(unsigned io, io_special_t special)
{
    return ((      io == W1_PULSE_1_IO                      ||      io == W1_PULSE_2_IO                         ) &&
            ( special == IO_SPECIAL_ONEWIRE                 || special == IO_SPECIAL_PULSECOUNT_RISING_EDGE ||
              special == IO_SPECIAL_PULSECOUNT_FALLING_EDGE || special == IO_SPECIAL_PULSECOUNT_BOTH_EDGE   ||
              special == IO_SPECIAL_WATCH                   ));
}


void model_uarts_setup(void)
{
    uint32_t reg32 = RCC_CCIPR & ~(RCC_CCIPR_LPUART1SEL_MASK << RCC_CCIPR_LPUART1SEL_SHIFT);
    RCC_CCIPR = reg32 | ((RCC_CCIPR_LPUART1SEL_HSI16 & RCC_CCIPR_LPUART1SEL_MASK) << RCC_CCIPR_LPUART1SEL_SHIFT);
}


void model_setup_pulse_pupd(uint8_t* pupd)
{
}


unsigned model_measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,         100,  1, FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CONFIG_REVISION,    100,  1, CONFIG_REVISION );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM1_0_NAME,           0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM25_NAME,            0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM4_NAME,             0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM10_NAME,            0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_REL_HUM_NAME,         0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SEN54_TEMP_NAME,      0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_VOC_NAME,             0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_NOX_NAME,             0,  5, SEN54           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  1, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  1, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  1, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_W1_PROBE_NAME_1,      0,  1, W1_PROBE        );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1, PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1, PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_LIGHT_NAME,           1,  1, LIGHT           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SOUND_NAME,           1,  1, SOUND           );
    return pos;
}


bool __attribute__((weak)) bat_on_battery(bool* on_battery)
{
    /* No battery fitted */
    return false;
}
