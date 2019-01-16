#ifndef __PINMAPS__
#define __PINMAPS__

#define ADCS_GPIOA  GPIOA
#define ADCS_A_GPIO4_GPIO6_GPIO7                     (GPIO4 | GPIO6 | GPIO7)

#define ADCS_GPIOB  GPIOB
#define ADCS_B_GPIO0_GPIO1                           (GPIO0 | GPIO1)

#define ADCS_GPIOC  GPIOC
#define ADCS_C_GPIO0_GPIO1_GPIO2_GPIO3_GPIO4_GPIO5   (GPIO0 | GPIO1 | GPIO2 | GPIO3 | GPIO4| GPIO5)


#define PPS1_EXTI2_3_ISR        exti2_3_isr
#define PPS1_NVIC_EXTI2_3_IRQ   NVIC_EXTI2_3_IRQ
#define PPS1_EXTI3              EXTI3
#define PPS1_GPIOB_RCC          RCC_GPIOB
#define PPS1_GPIOB              GPIOB
#define PPS1_GPIO3              GPIO3

#define PPS2_EXTI4_15_ISR       exti4_15_isr
#define PPS2_NVIC_EXTI4_15_IRQ  NVIC_EXTI4_15_IRQ
#define PPS2_EXTI7              EXTI7
#define PPS2_GPIOC_RCC          RCC_GPIOC
#define PPS2_GPIOC              GPIOC
#define PPS2_GPIO7              GPIO7


#ifdef STM32F0
#define UART_CHANNELS                                                                                              \
{                                                                                                                  \
    { USART2, RCC_USART2, RCC_GPIOA, GPIOA, GPIO2  | GPIO3,  GPIO_AF1, NVIC_USART1_IRQ,   115200, UART1_PRIORITY },\
    { USART1, RCC_USART1, RCC_GPIOA, GPIOA, GPIO9  | GPIO10, GPIO_AF1, NVIC_USART2_IRQ,   115200, UART2_PRIORITY },\
    { USART3, RCC_USART3, RCC_GPIOC, GPIOC, GPIO10 | GPIO11, GPIO_AF1, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY },\
    { USART4, RCC_USART4, RCC_GPIOA, GPIOA, GPIO1  | GPIO0,  GPIO_AF4, NVIC_USART3_4_IRQ, 9600,   UART3_PRIORITY },\
}
#else
#define UART_CHANNELS                                                                                                   \
{                                                                                                                       \
    { USART2, RCC_USART2, RCC_GPIOA, GPIOA, GPIO2  | GPIO3,  GPIO_AF7, NVIC_USART2_EXTI26_IRQ, 115200, UART1_PRIORITY },\
    { USART1, RCC_USART1, RCC_GPIOA, GPIOA, GPIO9  | GPIO10, GPIO_AF7, NVIC_USART1_EXTI25_IRQ, 115200, UART2_PRIORITY },\
    { USART3, RCC_USART3, RCC_GPIOB, GPIOB, GPIO8  | GPIO9,  GPIO_AF7, NVIC_USART3_EXTI28_IRQ, 9600  , UART3_PRIORITY },\
    { UART4,  RCC_UART4,  RCC_GPIOC, GPIOC, GPIO10 | GPIO11, GPIO_AF5, NVIC_UART4_EXTI34_IRQ,  9600  , UART4_PRIORITY },\
}
#endif



#endif //__PINMAPS__
