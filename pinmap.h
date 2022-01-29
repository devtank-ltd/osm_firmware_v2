#pragma once

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <stdint.h>

#include "base_types.h"

#define I2C_BUSES {{RCC_I2C1, I2C1, i2c_speed_sm_100k, 8, GPIO_AF4, {GPIOB, GPIO8|GPIO9} }}

#define HTU21D_I2C       I2C1
#define HTU21D_I2C_INDEX 0

#define I2C_HTU21D_ADDR 0x40

#define LED_PORT   GPIOA
#define LED_PIN    GPIO0

#define ADCS_PORT_N_PINS                            \
{                                                   \
    {GPIOA, GPIO1},      /* ADC 1  = Channel 6  */  \
    {GPIOC, GPIO0},      /* ADC 1  = Channel 1  */  \
    {GPIOC, GPIO2},      /* ADC 1  = Channel 3  */  \
    {GPIOC, GPIO3},      /* ADC 1  = Channel 4  */  \
}

#define ADC1_CHANNEL__CURRENT_CLAMP      6
#define ADC1_CHANNEL__BAT_MON            1
#define ADC1_CHANNEL__3V3_RAIL_MON       3
#define ADC1_CHANNEL__5V_RAIL_MON        4

#define ADC_INDEX__CURRENT_CLAMP   0
#define ADC_INDEX__BAT_MON         1
#define ADC_INDEX__3V3_RAIL_MON    2
#define ADC_INDEX__5V_RAIL_MON     3

#define ADC_CHANNELS  { ADC1_CHANNEL__CURRENT_CLAMP,    \
                        ADC1_CHANNEL__BAT_MON,          \
                        ADC1_CHANNEL__3V3_RAIL_MON,     \
                        ADC1_CHANNEL__5V_RAIL_MON       }

#define ADC_COUNT 4

#define ADC_DMA_CHANNELS                                                        \
{                                                                               \
    { ADC1, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL1_IRQ, DMA_CHANNEL1, ADC_PRIORITY  , true } , /* ADC1 */   \
}

#define ADC_DMA_CHANNELS_COUNT  1

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

#define UART_CHANNELS                                                                                                   \
{                                                                                                                       \
    { USART2, RCC_USART2, UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, GPIOA, GPIO2|GPIO3,   GPIO_AF7, NVIC_USART2_IRQ, (uint32_t)&USART2_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL7_IRQ, DMA_CHANNEL7, UART2_PRIORITY, true }, /* UART 0 Debug */ \
    { USART3, RCC_USART3, UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, GPIOC, GPIO4|GPIO5,   GPIO_AF7, NVIC_USART3_IRQ, (uint32_t)&USART3_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL2_IRQ, DMA_CHANNEL2, UART3_PRIORITY, true }, /* UART 1 LoRa */ \
    { USART1, RCC_USART1, UART_1_SPEED, UART_1_DATABITS, UART_1_PARITY, UART_1_STOP, GPIOB, GPIO6|GPIO7,   GPIO_AF7, NVIC_USART1_IRQ, (uint32_t)&USART1_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL5_IRQ, DMA_CHANNEL5, UART1_PRIORITY, true }, /* UART 2 HPM */ \
    { UART4,  RCC_UART4,  UART_4_SPEED, UART_4_DATABITS, UART_4_PARITY, UART_4_STOP, GPIOC, GPIO10|GPIO11, GPIO_AF8, NVIC_UART4_IRQ,  (uint32_t)&UART4_TDR,  DMA2, RCC_DMA2, NVIC_DMA2_CHANNEL3_IRQ, DMA_CHANNEL3, UART4_PRIORITY, true }, /* UART 3 485 */ \
}

#define CMD_UART   0
#define LW_UART    1
#define HPM_UART   2
#define RS485_UART 3

#define UART_CHANNELS_COUNT 4

#define MODBUS_SPEED    UART_4_SPEED
#define MODBUS_PARITY   UART_4_PARITY
#define MODBUS_DATABITS UART_4_DATABITS
#define MODBUS_STOP     UART_4_STOP


#define IOS_PORT_N_PINS            \
{                                  \
    {GPIOC, GPIO6 },   /* IO 0  */ \
    {GPIOC, GPIO8 },   /* IO 1  */ \
    {GPIOB, GPIO2 },   /* IO 2  */ \
    {GPIOC, GPIO9 },   /* IO 3  */ \
    {GPIOB, GPIO3 },   /* IO 4  */ \
    {GPIOA, GPIO11 },  /* IO 5  */ \
    {GPIOA, GPIO12 },  /* IO 6  */ \
    {GPIOC, GPIO7 },   /* IO 7  */ \
    {GPIOB, GPIO4 },   /* IO 8  */ \
    {GPIOB, GPIO5 },   /* IO 9  */ \
    {GPIOB, GPIO14 },  /* IO 10 */ \
    {GPIOD, GPIO2 },   /* IO 11 */ \
}

/*
Schematic name, STM pin
GPIO01 C6                   IO 0
GPIO02 C8                   IO 1
GPIO03 B2                   IO 2
GPIO04 C9                   IO 3
GPIO06 B3                   IO 4
GPIO07 C12  - RE_485
GPIO08 A11  - ONE WIRE      IO 5
GPIO09 A12  - PULSE_COUNT   IO 6
GPIO10 A15  - DE_485
GPIO11 C7                   IO 7
GPIO12 B4                   IO 8
GPIO13 B5                   IO 9
GPIO14 B14                  IO 10
GPIO15 B15  - HPM_EN
GPIO16 D2                   IO 11
*/


#define IO_PULL_MASK    0x0003
#define IO_TYPE_MASK    0xF000

#define IO_AS_INPUT     0x0100
#define IO_DIR_LOCKED   0x0200
#define IO_SPECIAL_EN   0x0400

#define IO_PULSECOUNT   0x3000
#define IO_ONEWIRE      0x4000

#define IOS_STATE                                              \
{                                                              \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 0   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 1   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 2   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 3   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 4   */         \
    IO_AS_INPUT | IO_ONEWIRE,           /* GPIO 5   */         \
    IO_AS_INPUT | IO_PULSECOUNT,        /* GPIO 6   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 7   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 8   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 9   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 10  */         \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,   /* GPIO 11  */         \
}

#define W1_PORT             GPIOA
#define W1_PIN              GPIO11
#define W1_IO               5

#define PULSE_PORT     GPIOA
#define PULSE_PIN      GPIO12
#define PULSE_EXTI     EXTI12
#define PULSE_EXTI_IRQ NVIC_EXTI15_10_IRQ
#define PULSE_ISR      exti15_10_isr
#define PULSE_IO       6

#define HPM_EN_PIN  { GPIOB, GPIO15 }

#define RE_485_PIN  { GPIOC, GPIO12 }
#define DE_485_PIN  { GPIOA, GPIO15 }

#define MODBUS_RCC_TIM  RCC_TIM2
#define MODBUS_TIM      TIM2
#define MODBUS_RST_TIM  RST_TIM2


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
