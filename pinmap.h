#ifndef __PINMAPS__
#define __PINMAPS__

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
    {GPIOC, GPIO2},      /* ADC 7  = Channel 12 */  \
    {GPIOC, GPIO3},      /* ADC 8  = Channel 13 */  \
    {GPIOC, GPIO4},      /* ADC 9  = Channel 14 */  \
    {GPIOC, GPIO5},      /* ADC 10 = Channel 15 */  \
}


#define PPS_PORT_N_PINS             \
{                                   \
    {GPIOB, GPIO3},     /* PPS 0 */ \
    {GPIOC, GPIO7},     /* PPS 1 */ \
}


#define PPS_EXTI               \
{                              \
    {TIM1, EXTI3,   0, TIM15}, \
    {TIM14, EXTI7, -1,  -1},   \
}

#define PPS_INIT                                               \
{                                                              \
    {RCC_TIM1, NVIC_EXTI2_3_IRQ, RCC_TIM15, NVIC_TIM15_IRQ},   \
    {RCC_TIM14, NVIC_EXTI4_15_IRQ, -1, -1},                    \
}

#define PPS0_EXTI_ISR        exti2_3_isr
#define PPS1_EXTI_ISR        exti4_15_isr
#define PPS0_ADC_TIMER_ISR   tim15_isr

#define UART_CHANNELS                                                                                                 \
{                                                                                                                     \
    { USART2, RCC_USART2, GPIOA, GPIO2  | GPIO3,  GPIO_AF1, NVIC_USART1_IRQ,   115200, UART1_PRIORITY }, /* UART 0 */ \
    { USART1, RCC_USART1, GPIOA, GPIO9  | GPIO10, GPIO_AF1, NVIC_USART2_IRQ,   9600,   UART2_PRIORITY }, /* UART 1 */ \
    { USART3, RCC_USART3, GPIOC, GPIO10 | GPIO11, GPIO_AF1, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY }, /* UART 2 */ \
    { USART4, RCC_USART4, GPIOA, GPIO1  | GPIO0,  GPIO_AF4, NVIC_USART3_4_IRQ, 115200, UART3_PRIORITY }, /* UART 3 */ \
}

#define INPUTS_PORT_N_PINS              \
{                                       \
    {GPIOC, GPIO12},   /* Input 0 */    \
    {GPIOA, GPIO15},   /* Input 1 */    \
    {GPIOB, GPIO7},    /* Input 2 */    \
    {GPIOA, GPIO8},    /* Input 3 */    \
    {GPIOB, GPIO10},   /* Input 4 */    \
    {GPIOB, GPIO4},    /* Input 5 */    \
    {GPIOD, GPIO2},    /* Input 6 */    \
    {GPIOF, GPIO1},    /* Input 7 */    \
    {GPIOB, GPIO13},   /* Input 8 */    \
}



#define OUTPUTS_PORT_N_PINS              \
{                                        \
    {GPIOC, GPIO8},     /* Output 0 */   \
    {GPIOC, GPIO6},     /* Output 1 */   \
    {GPIOB, GPIO12},    /* Output 2 */   \
    {GPIOB, GPIO11},    /* Output 3 */   \
    {GPIOB, GPIO2},     /* Output 4 */   \
    {GPIOB, GPIO6},     /* Output 5 */   \
    {GPIOB, GPIO14},    /* Output 6 */   \
    {GPIOB, GPIO15},    /* Output 7 */   \
}

#define OUTPUT_PULL                      \
{                                        \
    GPIO_PUPD_PULLDOWN,                  \
    GPIO_PUPD_PULLDOWN,                  \
    GPIO_PUPD_PULLDOWN,                  \
    GPIO_PUPD_PULLDOWN,                  \
    GPIO_PUPD_PULLDOWN,                  \
    GPIO_PUPD_PULLUP,                    \
    GPIO_PUPD_PULLUP,                    \
    GPIO_PUPD_PULLUP,                    \
}

#endif //__PINMAPS__
