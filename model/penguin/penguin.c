typedef int iso_is_annoying_go_away_pls_t;
#include <string.h>
#include <signal.h>

#include "timers.h"
#include "io.h"
#include "adcs.h"
#include "can_impl.h"
#include "log.h"
#include "config.h"
#include "pinmap.h"
#include "uart_rings.h"
#include "hpm.h"
#include "cc.h"
#include "bat.h"
#include "modbus_measurements.h"
#include "ds18b20.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "veml7700.h"
#include "sai.h"
#include "fw.h"
#include "persist_config.h"
#include "debug_mode.h"
#include "sleep.h"
#include "update.h"
#include "modbus.h"
#include "io_watch.h"

#include "peripherals.h"

#include "platform.h"
#include "model.h"
#include "linux.h"


void penguin_persist_config_model_init(persist_penguin_config_v1_t* model_config)
{
    model_config->mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
    cc_setup_default_mem(model_config->cc_configs, sizeof(cc_config_t) * ADC_CC_COUNT);
    ftma_setup_default_mem(model_config->ftma_configs, sizeof(ftma_config_t) * ADC_FTMA_COUNT);
    model_config->sai_no_buf = SAI_DEFAULT_NO_BUF;
}


/* Return true  if different
 *        false if same      */
bool penguin_persist_config_cmp(persist_penguin_config_v1_t* d0, persist_penguin_config_v1_t* d1)
{
    return !(
        d0 && d1 &&
        d0->mins_interval == d1->mins_interval &&
        !modbus_persist_config_cmp(&d0->modbus_bus, &d1->modbus_bus) &&
        memcmp(d0->cc_configs, d1->cc_configs, sizeof(cc_config_t) * ADC_CC_COUNT) == 0 &&
        memcmp(d0->ios_state, d1->ios_state, sizeof(uint16_t) * IOS_COUNT) == 0 &&
        memcmp(d0->sai_cal_coeffs, d1->sai_cal_coeffs, sizeof(float) * SAI_NUM_CAL_COEFFS) == 0 &&
        d0->sai_no_buf == d1->sai_no_buf );
}


void penguin_sensors_init(void)
{
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
    ftma_init();
}


void penguin_post_init(void)
{
    io_watch_init();
}


bool penguin_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring)
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


bool penguin_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring)
{
    if (uart == EXT_UART)
        return modbus_uart_ring_do_out_drain(ring);
    return true;
}


bool penguin_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
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
        case FTMA:          ftma_inf_init(inf);        break;
        case IO_READING:    ios_inf_init(inf);         break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


void penguin_debug_mode_enable_all(void)
{
    adcs_type_t all_cc_channels[ADC_CC_COUNT] = ADC_TYPES_ALL_CC;
    cc_set_active_clamps(all_cc_channels, ADC_CC_COUNT);
}


void penguin_measurements_repopulate(void)
{
    measurements_repop_indiv(MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_repop_indiv(MEASUREMENTS_CONFIG_REVISION,      4,  1,  CONFIG_REVISION );
    measurements_repop_indiv(MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_repop_indiv(MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_repop_indiv(MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_repop_indiv(MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_repop_indiv(MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_repop_indiv(MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_repop_indiv(MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_repop_indiv(MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_repop_indiv(MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_repop_indiv(MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    measurements_repop_indiv(MEASUREMENTS_FTMA_1_NAME,          0,  25, FTMA            );
    measurements_repop_indiv(MEASUREMENTS_FTMA_2_NAME,          0,  25, FTMA            );
    measurements_repop_indiv(MEASUREMENTS_FTMA_3_NAME,          0,  25, FTMA            );
    measurements_repop_indiv(MEASUREMENTS_FTMA_4_NAME,          0,  25, FTMA            );
}


void penguin_cmds_add_all(struct cmd_link_t* tail)
{
    tail = bat_add_commands(tail);
    tail = cc_add_commands(tail);
    tail = can_impl_add_commands(tail);
    tail = sai_add_commands(tail);
    tail = persist_config_add_commands(tail);
    tail = measurements_add_commands(tail);
    tail = ios_add_commands(tail);
    tail = debug_mode_add_commands(tail);
    tail = modbus_add_commands(tail);
    tail = sleep_add_commands(tail);
    tail = update_add_commands(tail);
    tail = comms_add_commands(tail);
    tail = ftma_add_commands(tail);
}


void penguin_w1_pulse_enable_pupd(unsigned io, bool enabled)
{
}


bool penguin_can_io_be_special(unsigned io, io_special_t special)
{
    return ((      io == W1_PULSE_1_IO                      ||      io == W1_PULSE_2_IO                         ) &&
            ( special == IO_SPECIAL_ONEWIRE                 || special == IO_SPECIAL_PULSECOUNT_RISING_EDGE ||
              special == IO_SPECIAL_PULSECOUNT_FALLING_EDGE || special == IO_SPECIAL_PULSECOUNT_BOTH_EDGE   ||
              special == IO_SPECIAL_WATCH                   ));
}


unsigned penguin_measurements_add_defaults(measurements_def_t * measurements_arr)
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
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FTMA_1_NAME,          0,  25, FTMA            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FTMA_2_NAME,          0,  25, FTMA            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FTMA_3_NAME,          0,  25, FTMA            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FTMA_4_NAME,          0,  25, FTMA            );
    return pos;
}


static unsigned _penguin_pids[5] = {0};


void penguin_linux_spawn_fakes(void)
{
    peripherals_add_cmd(&_penguin_pids[0]);
    peripherals_add_modbus(EXT_UART , &_penguin_pids[1]);
    peripherals_add_hpm(HPM_UART    , &_penguin_pids[2]);
    peripherals_add_w1(1000000      , &_penguin_pids[3]);
    peripherals_add_i2c(2000000     , &_penguin_pids[4]);
}


void penguin_linux_close_fakes(void)
{
    for(unsigned n=0; n<ARRAY_SIZE(_penguin_pids); n++)
    {
        if (_penguin_pids[n])
        {
            linux_port_debug("Killing child PID %u", _penguin_pids[n]);
            kill(_penguin_pids[n], SIGINT);
        }
    }
}

