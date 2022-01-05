#ifndef __PINMAPS__
#define __PINMAPS__

#include "base_types.h"

#define ERR_UART           USART2
#define ERR_UART_RCC       RCC_USART2
#define ERR_UART_GPIO      GPIOA
#define ERR_UART_PINS      (GPIO2|GPIO3)
#define ERR_UART_GPIO_AF   GPIO_AF7

#define LED_PORT   GPIOA
#define LED_PIN    GPIO0

#endif //__PINMAPS__
