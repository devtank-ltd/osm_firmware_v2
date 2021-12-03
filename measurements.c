#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/cortex.h>

#include "measurements.h"
#include "lorawan.h"
#include "log.h"
#include "config.h"
#include "hpm.h"
#include "sys_time.h"


#define MEASUREMENTS__UNSET_VALUE   UINT32_MAX
#define MEASUREMENTS__STR_SIZE      16

#define MEASUREMENTS__PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_AVERAGED     (uint8_t)0x02


#define MEASUREMENTS__EXPONENT_TEMPERATURE      -3
#define MEASUREMENTS__EXPONENT_HUMIDITY         -1
#define MEASUREMENTS__EXPONENT_PM10              0
#define MEASUREMENTS__EXPONENT_PM25              0


#define MEASUREMENTS__COLLECT_TIME__TEMPERATURE__MS 10000
#define MEASUREMENTS__COLLECT_TIME__HUMIDITY__MS    10000
#define MEASUREMENTS__COLLECT_TIME__HPM__MS         10000


typedef struct
{
    value_t     sum;
    value_t     max;
    value_t     min;
    uint8_t     num_samples;
    uint8_t     num_samples_init;
    uint8_t     num_samples_collected;
} measurement_data_t;


typedef struct
{
    measurement_def_t   def[LW__MAX_MEASUREMENTS];
    measurement_data_t  data[LW__MAX_MEASUREMENTS];
    uint16_t            len;
} measurement_arr_t;


typedef union
{
    uint32_t d;
    struct
    {
        uint8_t ll;
        uint8_t lh;
        uint8_t hl;
        uint8_t hh;
    };
} uint32_to_uint8_t;


typedef union
{
    uint16_t d;
    struct
    {
        uint8_t l;
        uint8_t h;
    };
} uint16_to_uint8_t;


static uint32_t last_sent_ms = 0;
static uint32_t interval_count = 0;
static uint8_t measurements_hex_arr[MEASUREMENTS__HEX_ARRAY_SIZE] = {0};
static uint16_t measurements_hex_arr_pos = 0;
static measurement_arr_t measurement_arr = {0};


uint32_t hpm_init_time = 0;
uint16_t hpm_pm10, hpm_pm25;


static void hpm_preinit(void)
{
    uint32_t now = since_boot_ms;
    if (hpm_init_time == 0 || since_boot_delta(now, hpm_init_time) > MEASUREMENTS__COLLECT_TIME__HPM__MS)
    {
        hpm_enable(true);
        hpm_request();
        hpm_init_time = now;
    }
}


static bool hpm_pm10_init(void)
{
    hpm_preinit();
    return true;
}


static bool hpm_pm10_get(value_t* value)
{
    *value = 0;
    uint16_t hpm_pm10_cp, hpm_pm25_cp;
    if (hpm_get(&hpm_pm10_cp, &hpm_pm25_cp))
    {
        hpm_pm10 = hpm_pm10_cp;
        hpm_pm25 = hpm_pm25_cp;
    }
    *value = hpm_pm10;
    return true;
}


static bool hpm_pm25_init(void)
{
    hpm_preinit();
    return true;
}


static bool hpm_pm25_get(value_t* value)
{
    *value = 0;
    uint16_t hpm_pm10_cp, hpm_pm25_cp;
    if (hpm_get(&hpm_pm10_cp, &hpm_pm25_cp))
    {
        hpm_pm10 = hpm_pm10_cp;
        hpm_pm25 = hpm_pm25_cp;
    }
    *value = hpm_pm25;
    return true;
}


static bool measurements_get_measurement_def(char* name, measurement_def_t* measurement_def)
{
    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        measurement_def = &measurement_arr.def[i];
        if (strncmp(measurement_def->name, name, strlen(measurement_def->name)) == 0)
        {
            return true;
        }
    }
    return false;
}


static uint8_t measurements_arr_append(uint8_t val)
{
    if (measurements_hex_arr_pos >= MEASUREMENTS__HEX_ARRAY_SIZE)
    {
        log_error("Measurement array is full.");
        return 1;
    }
    measurements_hex_arr[measurements_hex_arr_pos++] = val;
    return 0;
}


static bool measurements_to_arr(measurement_def_t* measurement_def, measurement_data_t* measurement_data)
{
    bool single = measurement_def->samplecount == 1;
    uint16_to_uint8_t mean;
    mean.d = measurement_data->sum / measurement_data->num_samples;

    uint8_t r = 0;
    r += measurements_arr_append(measurement_def->lora_id);
    if (single)
    {
        r += measurements_arr_append(MEASUREMENTS__DATATYPE_SINGLE);
        r += measurements_arr_append(mean.h);
        r += measurements_arr_append(mean.l);
    }
    else
    {
        uint16_to_uint8_t min;
        uint16_to_uint8_t max;
        min.d = measurement_data->min;
        max.d = measurement_data->max;
        r += measurements_arr_append(MEASUREMENTS__DATATYPE_AVERAGED);
        r += measurements_arr_append(mean.h);
        r += measurements_arr_append(mean.l);
        r += measurements_arr_append(min.h);
        r += measurements_arr_append(min.l);
        r += measurements_arr_append(max.h);
        r += measurements_arr_append(max.l);
    }
    return (r == 0);
}


static void measurements_send(void)
{
    uint16_t            num_qd = 0;
    measurement_def_t*  m_def;
    measurement_data_t* m_data;

    memset(measurements_hex_arr, 0, LW__MAX_MEASUREMENTS);
    measurements_hex_arr_pos = 0;

    if (!measurements_arr_append(MEASUREMENTS__PAYLOAD_VERSION))
    {
        return;
    }

    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        m_def = &measurement_arr.def[i];
        m_data = &measurement_arr.data[i];
        m_data->num_samples_init = 0;
        m_data->num_samples_collected = 0;
        if (m_def->interval && (interval_count % m_def->interval == 0))
        {
            if (m_data->sum == MEASUREMENTS__UNSET_VALUE || m_data->num_samples == 0)
            {
                log_error("Measurement requested but value not set.");
                continue;
            }
            if (measurements_to_arr(m_def, m_data))
            {
                num_qd++;
            }
        }
    }
    if (num_qd > 0)
    {
        lw_send(measurements_hex_arr, measurements_hex_arr_pos+1);
    }
}


static void measurements_sample(void)
{
    measurement_def_t*  m_def;
    measurement_data_t* m_data;
    uint32_t            sample_interval;
    value_t             new_value;
    uint32_t            now = since_boot_ms;
    uint32_t            time_since_interval;

    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        m_def  = &measurement_arr.def[i];
        m_data = &measurement_arr.data[i];

        sample_interval = m_def->interval * INTERVAL__TRANSMIT_MS / m_def->samplecount;
        time_since_interval = since_boot_delta(now, last_sent_ms);

        // If it takes time to get a sample, it is begun here.
        if (time_since_interval >= (m_data->num_samples_init * sample_interval) + sample_interval/2 - m_def->collection_time)
        {
            m_data->num_samples_init++;
            if (!m_def->init_cb())
            {
                log_error("Failed to call init for %s.", m_def->name);
                m_data->num_samples_collected++;
            }
        }

        // The sample is collected every interval/sample_rate but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^
        if (time_since_interval >= (m_data->num_samples_collected * sample_interval) + sample_interval/2)
        {
            m_data->num_samples_collected++;
            if (!m_def->get_cb(&new_value))
            {
                log_error("Could not get the %s value.", m_def->name);
                return;
            }
            if (m_data->sum == MEASUREMENTS__UNSET_VALUE)
            {
                m_data->sum = 0;
            }
            if (m_data->min == MEASUREMENTS__UNSET_VALUE)
            {
                m_data->min = new_value;
            }
            if (m_data->max == MEASUREMENTS__UNSET_VALUE)
            {
                m_data->max = new_value;
            }
            m_data->sum += new_value;
            m_data->num_samples++;
            if (new_value > m_data->max)
            {
                m_data->max = new_value;
            }
            else if (new_value < m_data->min)
            {
                m_data->min = new_value;
            }
        }
    }
}


uint16_t measurements_num_measurements(void)
{
    return measurement_arr.len;
}


bool measurements_add(measurement_def_t* measurement_def)
{
    if (measurement_arr.len >= LW__MAX_MEASUREMENTS)
    {
        log_error("Cannot add more measurements. Reached max.");
        return false;
    }
    measurement_data_t measurement_data = { MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, 0, 0, 0};
    measurement_arr.def[measurement_arr.len] = *measurement_def;
    measurement_arr.data[measurement_arr.len] = measurement_data;
    measurement_arr.len++;
    return true;
}


bool measurements_del(char* name)
{
    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        if (strcmp(name, measurement_arr.def[i].name) == 0)
        {
            memset(&measurement_arr.def[measurement_arr.len], 0, sizeof(measurement_def_t));
            measurement_arr.len--;
            return true;
        }
    }
    log_error("Cannot find measurement to remove.");
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    measurement_def_t* measurement_def = NULL;
    if (!measurements_get_measurement_def(name, measurement_def))
    {
        return false;
    }
    measurement_def->interval = interval;
    return true;
}


bool measurements_set_samplecount(char* name, uint8_t samplecount)
{
    if (samplecount == 0)
    {
        log_error("Cannot set the samplecount to 0.");
        return false;
    }
    measurement_def_t* measurement_def = NULL;
    if (!measurements_get_measurement_def(name, measurement_def))
    {
        return false;
    }
    measurement_def->samplecount = samplecount;
    return true;
}


void measurements_loop_iteration(void)
{
    uint32_t now = since_boot_ms;
    measurements_sample();

    if (since_boot_delta(now, last_sent_ms) > INTERVAL__TRANSMIT_MS)
    {
        if (interval_count > UINT32_MAX - 1)
        {
            interval_count = 0;
        }
        interval_count++;
        measurements_send();
        last_sent_ms = now;
    }
}


void measurements_init(void)
{
    measurement_def_t pm10_measurement_def = { "pm10" , LW_ID__PM10 , 10000 , 1, 5, hpm_pm10_init, hpm_pm10_get };
    measurement_def_t pm25_measurement_def = { "pm25" , LW_ID__PM25 , 10000 , 1, 5, hpm_pm25_init, hpm_pm25_get };

    measurements_add(&pm10_measurement_def);
    measurements_add(&pm25_measurement_def);
}
