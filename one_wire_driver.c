/*
A driver for a DS18B20 thermometer by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf                          (Accessed: 25.03.21)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html     (Accessed: 25.03.21)
*/
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <libopencm3/stm32/gpio.h>

#include "pinmap.h"
#include "log.h"

#define DELAY_READ_START    2
#define DELAY_READ_WAIT     10
#define DELAY_READ_RELEASE  50
#define DELAY_WRITE_1_START 6
#define DELAY_WRITE_1_END   64
#define DELAY_WRITE_0_START 60
#define DELAY_WRITE_0_END   10
#define DELAY_RESET_SET     500
#define DELAY_RESET_WAIT    60
#define DELAY_RESET_READ    480
#define DELAY_GET_TEMP      750000

#define W1_CMD_SKIP_ROM     0xCC
#define W1_CMD_CONV_T       0x44
#define W1_CMD_READ_SCP     0xBE

#define W1_PORT             GPIOA
#define W1_PIN              GPIO1

#define W1_DIRECTION_OUTPUT true
#define W1_DIRECTION_INPUT  false
#define W1_LEVEL_LOW        (uint8_t)0
#define W1_LEVEL_HIGH       (uint8_t)1


typedef union {
    struct {
        uint16_t t;
        uint16_t tmp_t;
        uint8_t conf;
        uint8_t res0;
        uint8_t res1;
        uint8_t res2;
        uint8_t crc;
    } __attribute__((packed));
    uint8_t raw[9];
} w1_memory_t;


static void w1_start_interrupt(void) {}

static void w1_stop_interrupt(void) {}


static void w1_delay_us(uint64_t delay) {
    delay = delay;
}


static void w1_set_direction(bool out) {
    if (out) {
        gpio_mode_setup(W1_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, W1_PIN);
    } else {
        gpio_mode_setup(W1_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, W1_PIN);
    }
}


static void w1_set_level(uint8_t bit) {
    if (bit) {
        gpio_set(W1_PORT, W1_PIN);
    } else {
        gpio_clear(W1_PORT, W1_PIN);
    }
}


static uint8_t w1_get_level(void) {
    if (gpio_get(W1_PORT, W1_PIN)) {
        return W1_LEVEL_HIGH;
    } else {
        return W1_LEVEL_LOW;
    }
}


static uint8_t w1_read_bit(void) {
    w1_start_interrupt(); 
    w1_set_direction(W1_DIRECTION_OUTPUT);
    w1_set_level(W1_LEVEL_LOW);
    w1_delay_us(DELAY_READ_START);
    w1_set_direction(W1_DIRECTION_INPUT);
    w1_delay_us(DELAY_READ_WAIT);
    uint8_t out = w1_get_level();
    w1_delay_us(DELAY_READ_RELEASE);
    w1_stop_interrupt();
    return out;
}


static void w1_send_bit(int bit) {
    w1_start_interrupt();
    if (bit) {
        w1_set_level(W1_LEVEL_LOW);
        w1_delay_us(DELAY_WRITE_1_START);
        w1_set_level(W1_LEVEL_HIGH);
        w1_delay_us(DELAY_WRITE_1_END);
    } else {
        w1_set_level(W1_LEVEL_LOW);
        w1_delay_us(DELAY_WRITE_0_START);
        w1_set_level(W1_LEVEL_HIGH);
        w1_delay_us(DELAY_WRITE_0_END); 
    }
    w1_stop_interrupt();
}


static bool w1_reset(void) {
    w1_set_direction(W1_DIRECTION_OUTPUT);
    w1_set_level(W1_LEVEL_LOW);
    w1_delay_us(DELAY_RESET_SET);
    w1_set_direction(W1_DIRECTION_INPUT);
    w1_delay_us(DELAY_RESET_WAIT);
    if (w1_get_level() == W1_LEVEL_HIGH) {
        return false;
        }
    w1_delay_us(DELAY_RESET_READ);
    if (w1_get_level() == W1_LEVEL_HIGH) {
        return false;
    } else {
        return true;
    }
}


static uint8_t w1_read_byte(void) {
    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; i++) {
        val = val + (w1_read_bit() << i);
    }
    return val;
}


static void w1_send_byte(uint8_t byte) {
    w1_set_direction(W1_DIRECTION_OUTPUT);
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & (1 << i)) {
            w1_send_bit(W1_LEVEL_HIGH);
        } else {
            w1_send_bit(W1_LEVEL_LOW);
        }
    }
    w1_set_direction(W1_DIRECTION_INPUT);
}


void w1_read_scpad(w1_memory_t* d) {
    for (int i = 0; i < 9; i++) {
        d->raw[i] = w1_read_byte();
    }
}


bool w1_crc_check(uint8_t* mem, uint8_t size) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < size; i++) {
         uint8_t byte = mem[i];
         for (uint8_t j = 0; j < 8; ++j) {
            uint8_t blend = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (blend) {
                crc ^= 0x8C;
            }
            byte >>= 1;
         }
    }
    if (crc == mem[size]) {
        return true;
    } else {
        return false;
    }
}


static void w1_temp_err(void) {
    log_debug(DEBUG_IO, "Temperature probe did not respond");
}


float w1_query_temp(void) {
    w1_memory_t d; 
    if (!w1_reset()) {
        w1_temp_err();
        return .0;
    }
    w1_send_byte(W1_CMD_SKIP_ROM);
    w1_send_byte(W1_CMD_CONV_T);
    w1_delay_us(DELAY_GET_TEMP);
    if (!w1_reset()) {
        w1_temp_err();
        return .0;
    }
    w1_send_byte(W1_CMD_SKIP_ROM);
    w1_send_byte(W1_CMD_READ_SCP);
    w1_read_scpad(&d);

    if (!w1_crc_check(d.raw, 8)) {
        log_debug(DEBUG_IO, "Temperature data not confirmed by CRC");
        return .0;
    }

    int16_t integer_bits = d.t >> 4;
    int8_t decimal_bits = d.t & 0x000F;
    if (d.t & 0x8000) {
        integer_bits = integer_bits | 0xF000;
        decimal_bits = (decimal_bits - 16) % 16;
    }

    float temperature = integer_bits + decimal_bits / 16.0f;
    log_debug(DEBUG_IO, "T: %.03f C", temperature);
    return temperature;
}


int w1_temp_init(void) {
    if (!w1_reset()) {
        w1_temp_err();
        return 1;
    }
    return 0;
}
