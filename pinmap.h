#ifndef __PINMAPS__
#define __PINMAPS__

#include <libopencm3/stm32/rcc.h>
#include <stdint.h>

typedef struct
{
    uint32_t port;
    uint32_t pins;
} port_n_pins_t;

#define PORT_TO_RCC(_port_)   (RCC_GPIOA + ((_port_ - GPIO_PORT_A_BASE) / 0x400))

#define LED_PORT   GPIOA
#define LED_PIN    GPIO5

#define ADCS_PORT_N_PINS                            \
{                                                   \
    {GPIOA, GPIO4},      /* ADC 0  = Channel 4  */  \
    {GPIOA, GPIO6},      /* ADC 1  = Channel 6  */  \
    {GPIOA, GPIO7},      /* ADC 2  = Channel 7  */  \
    {GPIOB, GPIO0},      /* ADC 3  = Channel 8  */  \
    {GPIOB, GPIO1},      /* ADC 4  = Channel 9  */  \
    {GPIOC, GPIO0},      /* ADC 5  = Channel 10 */  \
    {GPIOC, GPIO1},      /* ADC 6  = Channel 11 */  \
    {GPIOC, GPIO3},      /* ADC 7  = Channel 13 */  \
    {GPIOC, GPIO4},      /* ADC 8  = Channel 14 */  \
    {GPIOC, GPIO5},      /* ADC 9  = Channel 15 */  \
}

#define ADC_CHANNELS  {4,6,7,8,9,10,11,13,14,15}

#define ADC_COUNT 10

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


typedef enum
{
    uart_parity_none = 0,
    uart_parity_odd = 1,
    uart_parity_even = 2,
} uart_parity_t;

typedef enum
{
    uart_stop_bits_1 = 0,
    uart_stop_bits_1_5 = 1,
    uart_stop_bits_2 = 2,
} uart_stop_bits_t;

typedef struct
{
    uint32_t              usart;
    enum rcc_periph_clken uart_clk;
    uint32_t              baud;
    uint8_t               databits:4;
    uint8_t               parity:2 /*uart_parity_t*/;
    uint8_t               stop:2 /*uart_stop_bits_t*/;
    uint32_t              gpioport;
    uint16_t              pins;
    uint8_t               alt_func_num;
    uint8_t               irqn;
    uint32_t              dma_addr;
    uint8_t               dma_irqn;
    uint8_t               dma_channel;
    uint8_t               priority;
} uart_channel_t;

#define UART_CHANNELS                                                                                                   \
{                                                                                                                       \
    { USART2, RCC_USART2, UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, GPIOA, GPIO2  | GPIO3,  GPIO_AF1, NVIC_USART2_IRQ,   (uint32_t)&USART2_TDR, NVIC_DMA1_CHANNEL4_7_DMA2_CHANNEL3_5_IRQ, DMA_CHANNEL4, UART2_PRIORITY }, /* UART 0 */ \
    { USART3, RCC_USART3, UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, GPIOC, GPIO10 | GPIO11, GPIO_AF1, NVIC_USART3_4_IRQ, (uint32_t)&USART3_TDR, NVIC_DMA1_CHANNEL2_3_DMA2_CHANNEL1_2_IRQ, DMA_CHANNEL2, UART3_PRIORITY }, /* UART 1 */ \
    { USART4, RCC_USART4, UART_4_SPEED, UART_4_DATABITS, UART_4_PARITY, UART_4_STOP, GPIOA, GPIO1  | GPIO0,  GPIO_AF4, NVIC_USART3_4_IRQ, (uint32_t)&USART4_TDR, NVIC_DMA1_CHANNEL4_7_DMA2_CHANNEL3_5_IRQ, DMA_CHANNEL7, UART4_PRIORITY }, /* UART 2 */ \
}

#define UART_CHANNELS_COUNT 3
#define UART_DEBUG USART2

/*
 * NOTE: A13 and A14 will cause SWD problems.
 */
#define GPIOS_PORT_N_PINS               \
{                                       \
    {GPIOC, GPIO12},   /* Input 0 */    \
    {GPIOA, GPIO15},   /* Input 1 */    \
    {GPIOB, GPIO7},    /* Input 2 */    \
    {GPIOA, GPIO8},    /* Input 3 */    \
    {GPIOB, GPIO10},   /* Input 4 */    \
    {GPIOB, GPIO4},    /* Input 5 */    \
    {GPIOD, GPIO2},    /* Input 6 */    \
    {GPIOC, GPIO2},    /* Input 7 */    \
    {GPIOB, GPIO13},   /* Input 8 */    \
    {GPIOC, GPIO9},    /* Input 9 */    \
    {GPIOC, GPIO8},    /* Output 0 */  \
    {GPIOC, GPIO6},    /* Output 1 */  \
    {GPIOB, GPIO12},   /* Output 2 */  \
    {GPIOB, GPIO11},   /* Output 3 */  \
    {GPIOB, GPIO2},    /* Output 4 */  \
    {GPIOB, GPIO6},    /* Output 5 */  \
    {GPIOB, GPIO14},   /* Output 6 */  \
    {GPIOB, GPIO15},   /* Output 7 */  \
    {GPIOA, GPIO9},    /* Output 8 */  \
    {GPIOA, GPIO10},   /* Output 9 */  \
    {GPIOA, GPIO13},   /* Output 10 */ \
    {GPIOA, GPIO14},   /* Output 11 */ \
    {GPIOC, GPIO13},   /* Output 12 */ \
    {GPIOB, GPIO5},    /* Output 13 */ \
    {GPIOB, GPIO8},    /* Output 14 */ \
    {GPIOB, GPIO9},    /* Output 15 */ \
}


#define GPIO_AS_INPUT   0x100
#define GPIO_DIR_LOCKED 0x200
#define GPIO_PULL_MASK  0x0FF

#define GPIOS_STATE               \
{                                 \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 0 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 1 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 2 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 3 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 4 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 5 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 6 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 7 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 8 */  \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,    /* 9 */  \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 0  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 1  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 2  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 3  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 4  */ \
    GPIO_PUPD_PULLUP,                      /* 5  */ \
    GPIO_PUPD_PULLUP,                      /* 6  */ \
    GPIO_PUPD_PULLUP,                      /* 7  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 8  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 9  */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 10 */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 11 */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 12 */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 13 */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 14 */ \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN,  /* 15 */ \
}


#endif //__PINMAPS__
