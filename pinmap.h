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
    {GPIOC, GPIO0},      /* ADC 0  = Channel 1  */  \
    {GPIOC, GPIO1},      /* ADC 1  = Channel 2  */  \
    {GPIOC, GPIO2},      /* ADC 2  = Channel 3  */  \
    {GPIOC, GPIO3},      /* ADC 3  = Channel 4  */  \
}

#define ADC_CHANNELS  {1,2,3,4}

#define ADC_COUNT 4

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
    uint8_t               enabled;
} uart_channel_t;

#define UART_CHANNELS                                                                                                   \
{                                                                                                                       \
    { USART2, RCC_USART2, UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, GPIOA, GPIO2|GPIO3,  GPIO_AF7, NVIC_USART2_IRQ,   (uint32_t)&USART2_TDR, NVIC_DMA1_CHANNEL7_IRQ, DMA_CHANNEL7, UART2_PRIORITY, true }, /* UART 0 */ \
    { USART3, RCC_USART3, UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, GPIOC, GPIO4|GPIO5,  GPIO_AF7, NVIC_USART3_IRQ,   (uint32_t)&USART3_TDR, NVIC_DMA1_CHANNEL2_IRQ, DMA_CHANNEL2, UART3_PRIORITY, true }, /* UART 2 */ \
}

#define UART_CHANNELS_COUNT 2
#define UART_DEBUG USART2
//#define UART_LORAWAN USART3

#define IOS_PORT_N_PINS            \
{                                  \
    {GPIOC, GPIO12},   /* IO 0  */ \
    {GPIOA, GPIO15},   /* IO 1  */ \
    {GPIOB, GPIO7},    /* IO 2  */ \
    {GPIOB, GPIO10},   /* IO 3  */ \
    {GPIOB, GPIO4},    /* IO 4  */ \
    {GPIOD, GPIO2},    /* IO 5  */ \
    {GPIOC, GPIO2},    /* IO 6  */ \
    {GPIOB, GPIO13},   /* IO 7  */ \
    {GPIOC, GPIO9},    /* IO 8  */ \
    {GPIOC, GPIO8},    /* IO 9 */ \
    {GPIOC, GPIO6},    /* IO 10 */ \
    {GPIOB, GPIO12},   /* IO 11 */ \
    {GPIOB, GPIO11},   /* IO 12 */ \
    {GPIOB, GPIO2},    /* IO 13 */ \
    {GPIOB, GPIO6},    /* IO 14 */ \
    {GPIOB, GPIO14},   /* IO 15 */ \
    {GPIOB, GPIO15},   /* IO 16 */ \
}


#define IO_AS_INPUT     0x0100
#define IO_DIR_LOCKED   0x0200
#define IO_SPECIAL_EN   0x0400
#define IO_UART_TX      0x0800
#define IO_RELAY        0x1000
#define IO_HIGHSIDE     0x2000
#define IO_PPS0         0x3000
#define IO_PPS1         0x4000
#define IO_UART0        0x5000
#define IO_UART1        0x6000
#define IO_TYPE_MASK    0xF000
#define IO_PULL_MASK    0x0003

#define IOS_STATE                                                            \
{                                                                            \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 0   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 1   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 2   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 3   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 4   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 5   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 6   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 7   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 8   */         \
    IO_AS_INPUT | GPIO_PUPD_PULLUP,                   /* GPIO 9   */         \
    IO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | IO_RELAY,    /* GPIO 10  */         \
    IO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | IO_RELAY,    /* GPIO 11  */         \
    IO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | IO_RELAY,    /* GPIO 12  */         \
    IO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | IO_RELAY,    /* GPIO 13  */         \
    IO_DIR_LOCKED | GPIO_PUPD_PULLDOWN | IO_RELAY,    /* GPIO 14  */         \
    GPIO_PUPD_PULLUP,                                 /* GPIO 15  */         \
    GPIO_PUPD_PULLUP,                                 /* GPIO 16  */         \
}

#define PPS0_IO_NUM          26
#define PPS1_IO_NUM          27


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



#endif //__PINMAPS__
