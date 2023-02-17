#pragma once

#ifdef STM32L4
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#endif

#include <stdint.h>

#include "base_types.h"

#include "model_pinmap.h"

#define I2C_BUSES {{RCC_I2C1, I2C1, i2c_speed_sm_100k, 8, GPIO_AF4, {GPIOB, GPIO8|GPIO9} }}

#define HTU21D_I2C          I2C1
#define HTU21D_I2C_INDEX    0
#define VEML7700_I2C        I2C1
#define VEML7700_I2C_INDEX  0

#define LED_PORT   GPIOA
#define LED_PIN    GPIO0

#define ADCS_PORT_N_PINS                CONCAT(FW_NAME,_ADCS_PORT_N_PINS)

#define ADC1_CHANNEL_BAT_MON            CONCAT(FW_NAME,_ADC1_CHANNEL_BAT_MON)
#define ADC1_CHANNEL_3V3_RAIL_MON       CONCAT(FW_NAME,_ADC1_CHANNEL_3V3_RAIL_MON)
#define ADC1_CHANNEL_5V_RAIL_MON        CONCAT(FW_NAME,_ADC1_CHANNEL_5V_RAIL_MON)

#define ADC_DMA_CHANNELS                CONCAT(FW_NAME,_ADC_DMA_CHANNELS)
#define ADC_DMA_CHANNELS_COUNT          CONCAT(FW_NAME,_ADC_DMA_CHANNELS_COUNT)

#define ADC_COUNT                       CONCAT(FW_NAME,_ADC_COUNT)

#define PPS_PORT_N_PINS             \
{                                   \
    {GPIOB, GPIO3},     /* PPS 0 */ \
    {GPIOC, GPIO7},     /* PPS 1 */ \
}


#define PPS_EXTI      \
{                     \
    {TIM1, EXTI3},    \
    {TIM14, EXTI7},   \
}

#define PPS_INIT                     \
{                                    \
    {RCC_TIM1, NVIC_EXTI2_3_IRQ },   \
    {RCC_TIM14, NVIC_EXTI4_15_IRQ},  \
}

#define PPS0_EXTI_ISR        exti2_3_isr
#define PPS1_EXTI_ISR        exti4_15_isr

#define UART_CHANNELS                   CONCAT(FW_NAME,_UART_CHANNELS)
#define COMMS_RESET_PORT_N_PINS         CONCAT(FW_NAME,_COMMS_RESET_PORT_N_PINS)
#define COMMS_BOOT_PORT_N_PINS          CONCAT(FW_NAME,_COMMS_BOOT_PORT_N_PINS)

#define HPM_EN_PIN  { GPIOB, GPIO15 }

#define RE_485_PIN  { GPIOC, GPIO12 }
#define DE_485_PIN  { GPIOA, GPIO15 }

#define MODBUS_RCC_TIM  RCC_TIM2
#define MODBUS_TIM      TIM2
#define MODBUS_RST_TIM  RST_TIM2

#define CAN_RCC_TIM     RCC_TIM3
#define CAN_TIM         TIM3
#define CAN_RST_TIM     RST_TIM3


#define SAI_PORT_N_PINS                    \
{                                          \
    {GPIOA, GPIO8},      /* SCK_A       */ \
    {GPIOA, GPIO9},      /* FS_A (fs=ws)*/ \
    {GPIOA, GPIO10},     /* SD_A        */ \
}


#define SAI_PORT_N_PINS_AF \
{                          \
    GPIO_AF13,             \
    GPIO_AF13,             \
    GPIO_AF13,             \
}

#define CMD_UART   0
#define COMMS_UART 1
#define HPM_UART   2
#define EXT_UART   3

#define UART_CHANNELS_COUNT 4

#define UART_BUFFERS_INIT                \
char uart_0_in_buf[UART_0_IN_BUF_SIZE];  \
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];\
char uart_1_in_buf[UART_1_IN_BUF_SIZE];  \
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];\
char uart_2_in_buf[UART_2_IN_BUF_SIZE];  \
char uart_2_out_buf[UART_2_OUT_BUF_SIZE];\
char uart_3_in_buf[UART_3_IN_BUF_SIZE];  \
char uart_3_out_buf[UART_3_OUT_BUF_SIZE];

#define UART_IN_RINGS                                   \
{                                                       \
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),\
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),\
    RING_BUF_INIT(uart_2_in_buf, sizeof(uart_2_in_buf)),\
    RING_BUF_INIT(uart_3_in_buf, sizeof(uart_3_in_buf)),\
}

#define UART_OUT_RINGS                                    \
{                                                         \
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),\
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),\
    RING_BUF_INIT(uart_2_out_buf, sizeof(uart_2_out_buf)),\
    RING_BUF_INIT(uart_3_out_buf, sizeof(uart_3_out_buf)),}
