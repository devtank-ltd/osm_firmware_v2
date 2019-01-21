#ifndef __PINMAPS__
#define __PINMAPS__

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

#define PPS1_EXTI_ISR        exti2_3_isr
#define PPS1_NVIC_EXTI_IRQ   NVIC_EXTI2_3_IRQ
#define PPS1_EXTI            EXTI3
#define PPS1_PORT            GPIOB
#define PPS1_PINS            GPIO3

#define PPS2_EXTI_ISR        exti4_15_isr
#define PPS2_NVIC_EXTI_IRQ   NVIC_EXTI4_15_IRQ
#define PPS2_EXTI            EXTI7
#define PPS2_PORT            GPIOC
#define PPS2_PINS            GPIO7

#define UART_CHANNELS                                                                                   \
{                                                                                                       \
    { USART2, RCC_USART2, GPIOA, GPIO2  | GPIO3,  GPIO_AF1, NVIC_USART1_IRQ,   115200, UART1_PRIORITY },\
    { USART1, RCC_USART1, GPIOA, GPIO9  | GPIO10, GPIO_AF1, NVIC_USART2_IRQ,   115200, UART2_PRIORITY },\
    { USART3, RCC_USART3, GPIOC, GPIO10 | GPIO11, GPIO_AF1, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY },\
    { USART4, RCC_USART4, GPIOA, GPIO1  | GPIO0,  GPIO_AF4, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY },\
}

#define INPUTS_PORT_N_PINS              \
{                                       \
    {GPIOC, GPIO12},   /* Input 0 */    \
    {GPIOA, GPIO15},   /* Input 1 */    \
    {GPIOB, GPIO7},    /* Input 2 */    \
    {GPIOC, GPIO13},   /* Input 3 */    \
    {GPIOC, GPIO14},   /* Input 4 */    \
    {GPIOC, GPIO15},   /* Input 5 */    \
    {GPIOD, GPIO2},    /* Input 6 */    \
    {GPIOF, GPIO1},    /* Input 7 */    \
}



#define OUTPUTS_PORT_N_PINS              \
{                                        \
    {GPIOC, GPIO8},     /* Output 0 */   \
    {GPIOC, GPIO6},     /* Output 1 */   \
    {GPIOB, GPIO12},    /* Output 2 */   \
    {GPIOB, GPIO11},    /* Output 3 */   \
    {GPIOB, GPIO2},     /* Output 4 */   \
    {GPIOB, GPIO14},    /* Output 5 */   \
    {GPIOB, GPIO15},    /* Output 6 */   \
    {GPIOF, GPIO0},     /* Output 7 */    \
}

#endif //__PINMAPS__
