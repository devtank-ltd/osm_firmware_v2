#pragma once

#ifdef STM32L4
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#endif

#include <stdint.h>

#include "base_types.h"

#include "model_pinmap.h"

#define I2C_BUSES {{RCC_I2C1, I2C1, i2c_speed_sm_100k, 8, GPIO_AF4, {GPIOB, GPIO8|GPIO9} }}

#define HTU21D_I2C          I2C1
#define HTU21D_I2C_INDEX    0
#define VEML7700_I2C        I2C1
#define VEML7700_I2C_INDEX  0

#define I2C_HTU21D_ADDR     0x40
#define I2C_VEML7700_ADDR   0x10

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

#define UART_CHANNELS_REV_B                                                                                             \
{                                                                                                                       \
    { USART2, RCC_USART2, UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, GPIOA, GPIO2|GPIO3,   GPIO_AF7, NVIC_USART2_IRQ, (uint32_t)&USART2_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL7_IRQ, DMA_CHANNEL7, UART2_PRIORITY, true }, /* UART 0 Debug */ \
    { USART3, RCC_USART3, UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, GPIOC, GPIO4|GPIO5,   GPIO_AF7, NVIC_USART3_IRQ, (uint32_t)&USART3_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL2_IRQ, DMA_CHANNEL2, UART3_PRIORITY, true }, /* UART 1 LoRa */ \
    { USART1, RCC_USART1, UART_1_SPEED, UART_1_DATABITS, UART_1_PARITY, UART_1_STOP, GPIOB, GPIO6|GPIO7,   GPIO_AF7, NVIC_USART1_IRQ, (uint32_t)&USART1_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL5_IRQ, DMA_CHANNEL5, UART1_PRIORITY, true }, /* UART 2 HPM */ \
    { UART4,  RCC_UART4,  UART_4_SPEED, UART_4_DATABITS, UART_4_PARITY, UART_4_STOP, GPIOC, GPIO10|GPIO11, GPIO_AF8, NVIC_UART4_IRQ,  (uint32_t)&UART4_TDR,  DMA2, RCC_DMA2, NVIC_DMA2_CHANNEL3_IRQ, DMA_CHANNEL3, UART4_PRIORITY, true }, /* UART 3 485 */ \
}

#define UART_CHANNELS_REV_C                                                                                             \
{                                                                                                                       \
    { USART2,  RCC_USART2,  UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, GPIOA, GPIO2|GPIO3,   GPIO_AF7, NVIC_USART2_IRQ, (uint32_t)&USART2_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL7_IRQ, DMA_CHANNEL7, UART2_PRIORITY,   true }, /* UART 0 Debug */ \
    { USART3,  RCC_USART3,  UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, GPIOC, GPIO4|GPIO5,   GPIO_AF7, NVIC_USART3_IRQ, (uint32_t)&USART3_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL2_IRQ, DMA_CHANNEL2, UART3_PRIORITY,   true }, \
    { USART1,  RCC_USART1,  UART_1_SPEED, UART_1_DATABITS, UART_1_PARITY, UART_1_STOP, GPIOB, GPIO6|GPIO7,   GPIO_AF7, NVIC_USART1_IRQ, (uint32_t)&USART1_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL5_IRQ, DMA_CHANNEL5, UART1_PRIORITY,   true }, \
    { LPUART1, RCC_LPUART1, UART_4_SPEED, UART_4_DATABITS, UART_4_PARITY, UART_4_STOP, GPIOB, GPIO10|GPIO11, GPIO_AF8, NVIC_LPUART1_IRQ, (uint32_t)&USART_TDR(LPUART1_BASE), DMA2, RCC_DMA2, NVIC_DMA2_CHANNEL6_IRQ, DMA_CHANNEL6, UART4_PRIORITY, true }, \
}

#define W1_PULSE_1_EXTI             EXTI11
#define W1_PULSE_1_EXTI_IRQ NVIC_EXTI15_10_IRQ

#define W1_PULSE_2_EXTI             EXTI12
#define W1_PULSE_2_EXTI_IRQ NVIC_EXTI15_10_IRQ

#define W1_PULSE_ISR                exti15_10_isr

#define W1_PULSE_1_PULLUP_EN_PORT   GPIOC
#define W1_PULSE_1_PULLUP_EN_PIN    GPIO7
#define W1_PULSE_2_PULLUP_EN_PORT   GPIOC
#define W1_PULSE_2_PULLUP_EN_PIN    GPIO6

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

#define IOS_COUNT 10
#define W1_PULSE_1_IO               4
#define W1_PULSE_2_IO               5
