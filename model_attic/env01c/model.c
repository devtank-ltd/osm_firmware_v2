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
#include <osm/sensors/hpm.h>
#include <osm/sensors/cc.h>
#include <osm/sensors/bat.h>
#include <osm/core/modbus_measurements.h>
#include <osm/sensors/ds18b20.h>
#include <osm/sensors/htu21d.h>
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
#include <osm/comms/lw.h>
#include "comms_direct.h"


uint8_t model_stm_adcs_get_channel(adcs_type_t adcs_type)
{
    switch(adcs_type)
    {
        case ADCS_TYPE_BAT: return ADC1_CHANNEL_BAT_MON;
        case ADCS_TYPE_CC_CLAMP1: return ADC1_CHANNEL_CURRENT_CLAMP_1;
        case ADCS_TYPE_CC_CLAMP2: return ADC1_CHANNEL_CURRENT_CLAMP_2;
        case ADCS_TYPE_CC_CLAMP3: return ADC1_CHANNEL_CURRENT_CLAMP_3;
        default:
            break;
    }
    return 0;
}


void model_persist_config_model_init(persist_model_config_t* model_config)
{
    model_config->mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
    cc_setup_default_mem(model_config->cc_configs, sizeof(model_config->cc_configs));
    lw_config_init(&model_config->comms_config);
    model_config->sai_no_buf = SAI_DEFAULT_NO_BUF;
    for (unsigned i = 0; i < W1_PULSE_COUNT; i++)
    {
        model_config->pulsecount_debounces_ms[i] = 50;
    }
}


/* Return true  if different
 *        false if same      */
bool model_persist_config_cmp(persist_model_config_t* d0, persist_model_config_t* d1)
{
    return !(
        d0 && d1 &&
        d0->mins_interval == d1->mins_interval &&
        !modbus_persist_config_cmp(&d0->modbus_bus, &d1->modbus_bus) &&
        !comms_persist_config_cmp(&d0->comms_config, &d1->comms_config) &&
        memcmp(d0->cc_configs, d1->cc_configs, sizeof(cc_config_t) * ADC_CC_COUNT) == 0 &&
        memcmp(d0->ios_state, d1->ios_state, sizeof(uint16_t) * IOS_COUNT) == 0 &&
        memcmp(d0->sai_cal_coeffs, d1->sai_cal_coeffs, sizeof(float) * SAI_NUM_CAL_COEFFS) == 0 &&
        d0->sai_no_buf == d1->sai_no_buf &&
        memcmp(d0->pulsecount_debounces_ms, d1->pulsecount_debounces_ms, sizeof(uint32_t) * W1_PULSE_COUNT) == 0);
}


static void _model_core_3v3_init(void)
{
    const port_n_pins_t core_3v3_port_n_pins = CORE_3V3_EN_PORT_N_PINS;
    platform_gpio_setup(&core_3v3_port_n_pins, false, IO_PUPD_NONE);
    platform_gpio_set(&core_3v3_port_n_pins, true);
}


void model_sensors_init(void)
{
    _model_core_3v3_init();
    timers_init();
    ios_init();
    sai_init();
    adcs_init();
    cc_init();
    htu21d_init();
    veml7700_init();
    ds18b20_temp_init();
    sai_init();
    pulsecount_init();
    modbus_init();
    can_impl_init();
    comms_direct_init();
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
    else if (uart == HPM_UART)
    {
        hpm_ring_process(ring, line_buffer, CMD_LINELEN);
        return true;
    }

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
        case PM10:          hpm_pm10_inf_init(inf);    break;
        case PM25:          hpm_pm25_inf_init(inf);    break;
        case MODBUS:        modbus_inf_init(inf);      break;
        case CURRENT_CLAMP: cc_inf_init(inf);          break;
        case W1_PROBE:      ds18b20_inf_init(inf);     break;
        case HTU21D_TMP:    htu21d_temp_inf_init(inf); break;
        case HTU21D_HUM:    htu21d_humi_inf_init(inf); break;
        case BAT_MON:       bat_inf_init(inf);         break;
        case PULSE_COUNT:   pulsecount_inf_init(inf);  break;
        case LIGHT:         veml7700_inf_init(inf);    break;
        case SOUND:         sai_inf_init(inf);         break;
        case IO_READING:    ios_inf_init(inf);         break;
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
    measurements_repop_indiv(MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_repop_indiv(MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
    measurements_repop_indiv(MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_repop_indiv(MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_repop_indiv(MEASUREMENTS_W1_PROBE_NAME_2,      0,  5,  W1_PROBE        );
    measurements_repop_indiv(MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_repop_indiv(MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_repop_indiv(MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_repop_indiv(MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_repop_indiv(MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_repop_indiv(MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_repop_indiv(MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
}


void model_cmds_add_all(struct cmd_link_t* tail)
{
    tail = bat_add_commands(tail);
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
    tail = comms_direct_add_commands(tail);
}


void model_w1_pulse_enable_pupd(unsigned io, bool enabled)
{
    port_n_pins_t w1_pupd_en_pnp;
    switch (io)
    {
        case W1_PULSE_1_IO:
        {
            port_n_pins_t t = W1_PULSE_1_PULLUP_EN_PORT_N_PINS;
            memcpy(&w1_pupd_en_pnp, &t, sizeof(port_n_pins_t));
            break;
        }
        case W1_PULSE_2_IO:
        {
            port_n_pins_t t = W1_PULSE_2_PULLUP_EN_PORT_N_PINS;
            memcpy(&w1_pupd_en_pnp, &t, sizeof(port_n_pins_t));
            break;
        }
        default:
        {
            log_error("Given IO which is not a valid W1 to set a pullup.");
            return;
        }
    }

    rcc_periph_clock_enable(PORT_TO_RCC(w1_pupd_en_pnp.port));
    gpio_mode_setup(w1_pupd_en_pnp.port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, w1_pupd_en_pnp.pins);
    if (enabled)
        gpio_set(w1_pupd_en_pnp.port, w1_pupd_en_pnp.pins);
    else
        gpio_clear(w1_pupd_en_pnp.port, w1_pupd_en_pnp.pins);
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
    /* Slightly confusing setup due to hardware:
     * NONE:
     *  STM PUPD:   None
     *  HW PUPD:    Down/None
     *  Comments:   This will actually be biased down as the chip pulls
     *              it to ground.
     * UP:
     *  STM PUPD:   None
     *  HW PUPD:    Up
     *  Comments:   As hardware-pull-up is used for pull up, have no pull
     *              up on the STM.
     * DOWN:
     *  STM PUPD:   Down
     *  HW PUPD:    Down/None
     *  Comments:   No comments.
     */
    switch (*pupd)
    {
        case IO_PUPD_DOWN:
            break;
        case IO_PUPD_UP:
        case IO_PUPD_NONE:
        default:
            *pupd = IO_PUPD_NONE;
            break;
    }
}


unsigned model_measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_W1_PROBE_NAME_2,      0,  5,  W1_PROBE        );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    return pos;
}


void model_main_loop_iterate(void)
{
    comms_direct_loop_iterate();
}


comms_type_t comms_identify(void)
{
    return COMMS_TYPE_LW;
}
