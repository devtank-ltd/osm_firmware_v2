#pragma once


#define ENV01C_ADCS_PORT_N_PINS                      \
{                                                   \
    {GPIOA, GPIO1},      /* ADC 1  = Channel 6  */  \
    {GPIOB, GPIO1},      /* ADC 1  = Channel 16 */  \
    {GPIOA, GPIO4},      /* ADC 1  = Channel 9  */  \
    {GPIOC, GPIO0},      /* ADC 1  = Channel 1  */  \
    {GPIOC, GPIO1},      /* ADC 1  = Channel 2  */  \
    {GPIOC, GPIO2},      /* ADC 1  = Channel 3  */  \
    {GPIOC, GPIO3},      /* ADC 1  = Channel 4  */  \
}

#define ENV01C_ADC1_CHANNEL_CURRENT_CLAMP_1    6
#define ENV01C_ADC1_CHANNEL_CURRENT_CLAMP_2   16
#define ENV01C_ADC1_CHANNEL_CURRENT_CLAMP_3    9
#define ENV01C_ADC1_CHANNEL_BAT_MON            1
#define ENV01C_ADC1_CHANNEL_BAT_LVL_MON        2
#define ENV01C_ADC1_CHANNEL_3V3_RAIL_MON       3
#define ENV01C_ADC1_CHANNEL_5V_RAIL_MON        4

#define ENV01C_ADC_DMA_CHANNELS                                                  \
{                                                                               \
    { ADC1, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL1_IRQ, DMA_CHANNEL1, ADC_PRIORITY  , true } , /* ADC1 */   \
}

#define ENV01C_ADC_DMA_CHANNELS_COUNT  1

#define ENV01C_ADC_COUNT       7

#define CORE_3V3_EN_PORT_N_PINS     {GPIOB, GPIO3}


#define ADC_TYPES_ALL_CC { ADCS_TYPE_CC_CLAMP1,  \
                           ADCS_TYPE_CC_CLAMP2,  \
                           ADCS_TYPE_CC_CLAMP3   }

#define IOS_WATCH_IOS                       { W1_PULSE_1_IO, W1_PULSE_2_IO }
#define IOS_WATCH_COUNT 2

#define CAN_PORT_N_PINS_RX    {GPIOA, GPIO11} /* CAN1RX */
#define CAN_PORT_N_PINS_TX    {GPIOA, GPIO12} /* CAN1TX */
#define CAN_PORT_N_PINS_STDBY {GPIOB, GPIO14} /* GPIO14 */


#define CAN_CONFIG {                                                   \
    CAN1,                                                              \
    RCC_CAN1,                                                          \
    CAN_PORT_N_PINS_STDBY,                                             \
    CAN_PORT_N_PINS_RX,                                                \
    GPIO_AF9,                                                          \
    CAN_PORT_N_PINS_TX,                                                \
    GPIO_AF9,                                                          \
}


#define W1_PULSE_1_PORT_N_PINS              { GPIOC, GPIO11 }
#define W1_PULSE_1_PULLUP_EN_PORT_N_PINS    { GPIOC, GPIO6  }
#define W1_PULSE_1_IO                       1
#define W1_PULSE_1_EXTI                     EXTI11
#define W1_PULSE_1_EXTI_IRQ                 NVIC_EXTI15_10_IRQ
#define W1_PULSE_1_ISR                      exti15_10_isr


#define W1_PULSE_2_PORT_N_PINS              { GPIOB, GPIO5  }
#define W1_PULSE_2_PULLUP_EN_PORT_N_PINS    { GPIOC, GPIO7  }
#define W1_PULSE_2_IO                       2
#define W1_PULSE_2_EXTI                     EXTI5
#define W1_PULSE_2_EXTI_IRQ                 NVIC_EXTI9_5_IRQ
#define W1_PULSE_2_ISR                      exti9_5_isr


#define DS18B20_INSTANCES   {                                          \
    { { MEASUREMENTS_W1_PROBE_NAME_1, W1_PULSE_1_IO} ,                 \
        0 },                                                           \
    { { MEASUREMENTS_W1_PROBE_NAME_2, W1_PULSE_2_IO} ,                 \
        1 },                                                           \
}


#define W1_IOS                  { {.pnp=W1_PULSE_1_PORT_N_PINS, .io=W1_PULSE_1_IO }, {.pnp=W1_PULSE_2_PORT_N_PINS, .io=W1_PULSE_2_IO } }


#define IOS_PORT_N_PINS            \
{                                  \
    {GPIOC, GPIO9 },   /* IO 0  */ \
    {GPIOC, GPIO11 },  /* IO 1  */ \
    {GPIOB, GPIO5 },   /* IO 2  */ \
    {GPIOC, GPIO7 },   /* IO 3  */ \
    {GPIOB, GPIO4 },   /* IO 4  */ \
    {GPIOB, GPIO5 },   /* IO 5  */ \
    {GPIOD, GPIO2 },   /* IO 6  */ \
}


#define IOS_STATE                                                      \
{                                                                      \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 0   */ \
    IO_AS_INPUT,                                        /* GPIO 1   */ \
    IO_AS_INPUT,                                        /* GPIO 2   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 3   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 4   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 5   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 6   */ \
}


#define ENV01C_COMMS_RESET_PORT_N_PINS     { GPIOC, GPIO8 }
#define ENV01C_COMMS_BOOT_PORT_N_PINS      { GPIOB, GPIO2 }


#ifndef __CONFIGTOOL__

#define UART_1_SPEED 9600
#define UART_2_SPEED 115200
#define UART_3_SPEED 115200

#define uart0_in_isr                    usart2_isr
#define uart1_in_isr                    usart3_isr
#define uart2_in_isr                    usart1_isr
#define uart3_in_isr                    lpuart1_isr


#define uart0_dma_out_isr               dma1_channel7_isr
#define uart1_dma_out_isr               dma1_channel2_isr
#define uart2_dma_out_isr               dma1_channel5_isr
#define uart3_dma_out_isr               dma2_channel6_isr

#endif // __CONFIGTOOL__


#define ENV01C_UART_CHANNELS                                                                                            \
{                                                                                                                       \
    { USART2,  RCC_USART2,  UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, GPIOA, GPIO2|GPIO3,   GPIO_AF7, NVIC_USART2_IRQ, (uint32_t)&USART2_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL7_IRQ, DMA_CHANNEL7, UART2_PRIORITY,   true , 2 }, /* UART 0 Debug */ \
    { USART3,  RCC_USART3,  UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, GPIOC, GPIO4|GPIO5,   GPIO_AF7, NVIC_USART3_IRQ, (uint32_t)&USART3_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL2_IRQ, DMA_CHANNEL2, UART3_PRIORITY,   true , 2 }, \
    { USART1,  RCC_USART1,  UART_1_SPEED, UART_1_DATABITS, UART_1_PARITY, UART_1_STOP, GPIOB, GPIO6|GPIO7,   GPIO_AF7, NVIC_USART1_IRQ, (uint32_t)&USART1_TDR, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL5_IRQ, DMA_CHANNEL5, UART1_PRIORITY,   true , 2 }, \
    { LPUART1, RCC_LPUART1, UART_4_SPEED, UART_4_DATABITS, UART_4_PARITY, UART_4_STOP, GPIOB, GPIO10|GPIO11, GPIO_AF8, NVIC_LPUART1_IRQ, (uint32_t)&USART_TDR(LPUART1_BASE), DMA2, RCC_DMA2, NVIC_DMA2_CHANNEL6_IRQ, DMA_CHANNEL6, UART4_PRIORITY, true , 4 }, \
}
