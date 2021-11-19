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
#define LED_PIN    GPIO0

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
    uint32_t              dma_unit;
    uint32_t              dma_rcc;
    uint8_t               dma_irqn;
    uint8_t               dma_channel;
    uint8_t               priority;
    uint8_t               enabled;
} uart_channel_t;

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

#define IOS_PORT_N_PINS             \
{                                   \
    {GPIOC, GPIO6 },   /* IO 0  */ \
    {GPIOC, GPIO8 },   /* IO 1  */ \
    {GPIOB, GPIO2 },   /* IO 2  */ \
    {GPIOC, GPIO9 },   /* IO 3  */ \
    {GPIOB, GPIO3 },   /* IO 4  */ \
    {GPIOC, GPIO12 },  /* IO 5  */ \
    {GPIOA, GPIO11 },  /* IO 6  */ \
    {GPIOC, GPIO7 },   /* IO 7  */ \
    {GPIOB, GPIO4 },   /* IO 8  */ \
    {GPIOB, GPIO5 },   /* IO 9  */ \
    {GPIOB, GPIO14 },  /* IO 10 */ \
    {GPIOB, GPIO15 },  /* IO 11 */ \
    {GPIOD, GPIO2 },   /* IO 12 */ \
}

/*
Schematic name, STM pin
GPIO01 C6
GPIO02 C8
GPIO03 B2
GPIO04 C9
GPIO06 B3
GPIO07 C12
GPIO08 A11
GPIO11 C7
GPIO12 B4
GPIO13 B5
GPIO14 B14
GPIO15 B15
GPIO16 D2
*/


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
