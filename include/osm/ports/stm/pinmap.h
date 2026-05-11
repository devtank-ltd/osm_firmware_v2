#pragma once

#ifdef STM32L4
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#endif

#include <stdint.h>

#include <osm/core/base_types.h>

#include "model_pinmap.h"

#define OSM_I2C_BUSES {{RCC_I2C1, RST_I2C1, I2C1, i2c_speed_sm_100k, 8, GPIO_AF4, {GPIOB, GPIO8|GPIO9} }}

#define OSM_HTU21D_I2C          I2C1
#define OSM_HTU21D_I2C_INDEX    0
#define OSM_VEML7700_I2C        I2C1
#define OSM_VEML7700_I2C_INDEX  0

#define OSM_LED_PORT   GPIOA
#define OSM_LED_PIN    GPIO0

#define OSM_PPS_PORT_N_PINS             \
{                                   \
    {GPIOB, GPIO3},     /* PPS 0 */ \
    {GPIOC, GPIO7},     /* PPS 1 */ \
}


#define OSM_PPS_EXTI      \
{                     \
    {TIM1, EXTI3},    \
    {TIM14, EXTI7},   \
}

#define OSM_PPS_INIT                     \
{                                    \
    {RCC_TIM1, NVIC_EXTI2_3_IRQ },   \
    {RCC_TIM14, NVIC_EXTI4_15_IRQ},  \
}

#define OSM_PPS0_EXTI_ISR        exti2_3_isr
#define OSM_PPS1_EXTI_ISR        exti4_15_isr

#define OSM_HPM_EN_PIN  { GPIOB, GPIO15 }

#define OSM_RE_485_PIN  { GPIOC, GPIO12 }
#define OSM_DE_485_PIN  { GPIOA, GPIO15 }

#define OSM_MODBUS_RCC_TIM  RCC_TIM2
#define OSM_MODBUS_TIM      TIM2
#define OSM_MODBUS_RST_TIM  RST_TIM2

#define OSM_CAN_RCC_TIM     RCC_TIM3
#define OSM_CAN_TIM         TIM3
#define OSM_CAN_RST_TIM     RST_TIM3


#define OSM_SAI_PORT_N_PINS                    \
{                                          \
    {GPIOA, GPIO8},      /* SCK_A       */ \
    {GPIOA, GPIO9},      /* FS_A (fs=ws)*/ \
    {GPIOA, GPIO10},     /* SD_A        */ \
}


#define OSM_SAI_PORT_N_PINS_AF \
{                          \
    GPIO_AF13,             \
    GPIO_AF13,             \
    GPIO_AF13,             \
}

#define OSM_UART_CHANNELS_COUNT 4

#define OSM_UART_BUFFERS_INIT                \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];  \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];\
char uart_1_in_buf[UART_1_IN_BUF_SIZE];  \
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];\
char uart_2_in_buf[UART_2_IN_BUF_SIZE];  \
char uart_2_out_buf[UART_2_OUT_BUF_SIZE];\
char uart_3_in_buf[UART_3_IN_BUF_SIZE];  \
char uart_3_out_buf[UART_3_OUT_BUF_SIZE];

#define OSM_UART_IN_RINGS                                   \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),\
    RING_BUF_INIT(uart_2_in_buf, sizeof(uart_2_in_buf)),\
    RING_BUF_INIT(uart_3_in_buf, sizeof(uart_3_in_buf)),\
}

#define OSM_UART_OUT_RINGS                                    \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),\
    RING_BUF_INIT(uart_2_out_buf, sizeof(uart_2_out_buf)),\
    RING_BUF_INIT(uart_3_out_buf, sizeof(uart_3_out_buf)),}
