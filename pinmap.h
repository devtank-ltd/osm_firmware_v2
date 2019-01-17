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

#define ADCS_PORT_N_PINS                                           \
{                                                                  \
    {GPIOA, (GPIO4 | GPIO6 | GPIO7)},                              \
    {GPIOB, (GPIO0 | GPIO1)},                                      \
    {GPIOC, (GPIO0 | GPIO1 | GPIO2 | GPIO3 | GPIO4| GPIO5)},       \
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


#ifdef STM32F0
#define UART_CHANNELS                                                                                              \
{                                                                                                                  \
    { USART2, RCC_USART2, GPIOA, GPIO2  | GPIO3,  GPIO_AF1, NVIC_USART1_IRQ,   115200, UART1_PRIORITY },\
    { USART1, RCC_USART1, GPIOA, GPIO9  | GPIO10, GPIO_AF1, NVIC_USART2_IRQ,   115200, UART2_PRIORITY },\
    { USART3, RCC_USART3, GPIOC, GPIO10 | GPIO11, GPIO_AF1, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY },\
    { USART4, RCC_USART4, GPIOA, GPIO1  | GPIO0,  GPIO_AF4, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY },\
}
#else
#define UART_CHANNELS                                                                                                   \
{                                                                                                                       \
    { USART2, RCC_USART2, GPIOA, GPIO2  | GPIO3,  GPIO_AF7, NVIC_USART2_EXTI26_IRQ, 115200, UART1_PRIORITY },\
    { USART1, RCC_USART1, GPIOA, GPIO9  | GPIO10, GPIO_AF7, NVIC_USART1_EXTI25_IRQ, 115200, UART2_PRIORITY },\
    { USART3, RCC_USART3, GPIOB, GPIO8  | GPIO9,  GPIO_AF7, NVIC_USART3_EXTI28_IRQ, 9600  , UART3_PRIORITY },\
    { UART4,  RCC_UART4,  GPIOC, GPIO10 | GPIO11, GPIO_AF5, NVIC_UART4_EXTI34_IRQ,  9600  , UART4_PRIORITY },\
}
#endif


#define INPUTS_PORT_N_PINS        \
{                                 \
    {GPIOC, GPIO12},              \
    {GPIOA, GPIO13},              \
    {GPIOA, GPIO14},              \
    {GPIOA, GPIO15},              \
    {GPIOB, GPIO7},               \
    {GPIOC, GPIO13},              \
    {GPIOC, GPIO14},              \
    {GPIOC, GPIO15},              \
    {GPIOF, GPIO0},               \
}

#endif //__PINMAPS__
