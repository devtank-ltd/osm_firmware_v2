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

/*subset of usb_cdc_line_coding_bParityType*/
typedef enum
{
    uart_parity_none = 0,
    uart_parity_odd  = 1,
    uart_parity_even = 2,
} uart_parity_t;

/*matches usb_cdc_line_coding_bCharFormat*/
typedef enum
{
    uart_stop_bits_1   = 0,
    uart_stop_bits_1_5 = 1,
    uart_stop_bits_2   = 2,
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
#define GPIOS_PORT_N_PINS            \
{                                    \
    {GPIOC, GPIO12},   /* GPIO 0  */ \
    {GPIOA, GPIO15},   /* GPIO 1  */ \
    {GPIOB, GPIO7},    /* GPIO 2  */ \
    {GPIOA, GPIO8},    /* GPIO 3  */ \
    {GPIOB, GPIO10},   /* GPIO 4  */ \
    {GPIOB, GPIO4},    /* GPIO 5  */ \
    {GPIOD, GPIO2},    /* GPIO 6  */ \
    {GPIOC, GPIO2},    /* GPIO 7  */ \
    {GPIOB, GPIO13},   /* GPIO 8  */ \
    {GPIOC, GPIO9},    /* GPIO 9  */ \
    {GPIOC, GPIO8},    /* GPIO 10 */ \
    {GPIOC, GPIO6},    /* GPIO 11 */ \
    {GPIOB, GPIO12},   /* GPIO 12 */ \
    {GPIOB, GPIO11},   /* GPIO 13 */ \
    {GPIOB, GPIO2},    /* GPIO 14 */ \
    {GPIOB, GPIO6},    /* GPIO 15 */ \
    {GPIOB, GPIO14},   /* GPIO 16 */ \
    {GPIOB, GPIO15},   /* GPIO 17 */ \
    {GPIOA, GPIO9},    /* GPIO 18 */ \
    {GPIOA, GPIO10},   /* GPIO 19 */ \
    {GPIOA, GPIO13},   /* GPIO 20 */ \
    {GPIOA, GPIO14},   /* GPIO 21 */ \
    {GPIOC, GPIO13},   /* GPIO 22 */ \
    {GPIOB, GPIO5},    /* GPIO 23 */ \
    {GPIOB, GPIO8},    /* GPIO 24 */ \
    {GPIOB, GPIO9},    /* GPIO 25 */ \
    {GPIOB, GPIO3},    /* GPIO 26 - PPS 0 */ \
    {GPIOC, GPIO7},    /* GPIO 27 - PPS 1 */ \
    {GPIOC, GPIO10},   /* GPIO 28 - UART_1_RX */ \
    {GPIOC, GPIO11},   /* GPIO 29 - UART_1_TX */ \
    {GPIOA, GPIO0},    /* GPIO 30 - UART_2_RX  */ \
    {GPIOA, GPIO1},    /* GPIO 31 - UART_2_TX */ \
}


#define GPIO_AS_INPUT     0x0100
#define GPIO_DIR_LOCKED   0x0200
#define GPIO_SPECIAL_EN   0x0400
#define GPIO_UART_TX      0x0800
#define GPIO_RELAY        0x1000
#define GPIO_HIGHSIDE     0x2000
#define GPIO_PPS0         0x3000
#define GPIO_PPS1         0x4000
#define GPIO_UART0        0x5000
#define GPIO_UART1        0x6000
#define GPIO_TYPE_MASK    0xF000
#define GPIO_PULL_MASK    0x00FF

#define GPIOS_STATE                                                              \
{                                                                                \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 0   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 1   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 2   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 3   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 4   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 5   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 6   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 7   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 8   */         \
    GPIO_AS_INPUT | GPIO_PUPD_PULLUP,                     /* GPIO 9   */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_RELAY,    /* GPIO 10  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_RELAY,    /* GPIO 11  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_RELAY,    /* GPIO 12  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_RELAY,    /* GPIO 13  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_RELAY,    /* GPIO 14  */         \
    GPIO_PUPD_PULLUP,                                     /* GPIO 15  */         \
    GPIO_PUPD_PULLUP,                                     /* GPIO 16  */         \
    GPIO_PUPD_PULLUP,                                     /* GPIO 17  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 18  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 19  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 20  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 21  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 22  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 23  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 24  */         \
    GPIO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | GPIO_HIGHSIDE, /* GPIO 25  */         \
    GPIO_AS_INPUT | GPIO_PPS0 | GPIO_SPECIAL_EN,          /* GPIO 26 - PPS 0 */     \
    GPIO_AS_INPUT | GPIO_PPS1 | GPIO_SPECIAL_EN,          /* GPIO 27 - PPS 1 */     \
    GPIO_UART0 | GPIO_SPECIAL_EN,                         /* GPIO 28 - UART_0_RX */ \
    GPIO_UART0 | GPIO_UART_TX | GPIO_SPECIAL_EN,          /* GPIO 29 - UART_0_TX */ \
    GPIO_UART1 | GPIO_SPECIAL_EN,                         /* GPIO 30 - UART_1_RX */ \
    GPIO_UART1 | GPIO_UART_TX | GPIO_SPECIAL_EN,          /* GPIO 31 - UART_1_TX */ \
}
#endif //__PINMAPS__
