#pragma once


#define ENV01_ADCS_PORT_N_PINS                      \
{                                                   \
    {GPIOA, GPIO1},      /* ADC 1  = Channel 6  */  \
    {GPIOB, GPIO1},      /* ADC 1  = Channel 16 */  \
    {GPIOA, GPIO4},      /* ADC 1  = Channel 9  */  \
    {GPIOC, GPIO0},      /* ADC 1  = Channel 1  */  \
    {GPIOC, GPIO2},      /* ADC 1  = Channel 3  */  \
    {GPIOC, GPIO3},      /* ADC 1  = Channel 4  */  \
}

#define ENV01_ADC1_CHANNEL_CURRENT_CLAMP_1    6
#define ENV01_ADC1_CHANNEL_CURRENT_CLAMP_2   16
#define ENV01_ADC1_CHANNEL_CURRENT_CLAMP_3    9
#define ENV01_ADC1_CHANNEL_BAT_MON            1
#define ENV01_ADC1_CHANNEL_3V3_RAIL_MON       3
#define ENV01_ADC1_CHANNEL_5V_RAIL_MON        4

#define ENV01_ADC_DMA_CHANNELS                                                  \
{                                                                               \
    { ADC1, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL1_IRQ, DMA_CHANNEL1, ADC_PRIORITY  , true } , /* ADC1 */   \
}

#define ENV01_ADC_DMA_CHANNELS_COUNT  1

#define ENV01_ADC_COUNT       6
#define ADC_CC_COUNT    3


#define ADC_TYPES_ALL_CC { ADCS_TYPE_CC_CLAMP1,  \
                           ADCS_TYPE_CC_CLAMP2,  \
                           ADCS_TYPE_CC_CLAMP3   }
                           

#define CAN_PORT_N_PINS_RX    {GPIOB, GPIO12} /* CAN1RX */
#define CAN_PORT_N_PINS_TX    {GPIOB, GPIO13} /* CAN1TX */
#define CAN_PORT_N_PINS_STDBY {GPIOB, GPIO14} /* GPIO14 */


#define W1_PULSE_1_PORT_N_PINS      { GPIOA, GPIO11 }
#define W1_PULSE_2_PORT_N_PINS      { GPIOA, GPIO12 }


#define IOS_PORT_N_PINS            \
{                                  \
    {GPIOC, GPIO6 },   /* IO 0  */ \
    {GPIOB, GPIO2 },   /* IO 1 */ \
    {GPIOC, GPIO9 },   /* IO 2  */ \
    {GPIOB, GPIO3 },   /* IO 3  */ \
    {GPIOA, GPIO11 },  /* IO 4  */ \
    {GPIOA, GPIO12 },  /* IO 5  */ \
    {GPIOC, GPIO7 },   /* IO 6  */ \
    {GPIOB, GPIO4 },   /* IO 7  */ \
    {GPIOB, GPIO5 },   /* IO 8  */ \
    {GPIOD, GPIO2 },   /* IO 9 */ \
}


/*
Schematic name, STM pin
GPIO01 C6                   IO 0
GPIO02 C8                   IO 1
GPIO03 B2                   IO 2
GPIO04 C9                   IO 3
GPIO06 B3                   IO 4
GPIO07 C12  - RE_485
GPIO08 A11  - PULSE2_OUT/1W IO 5
GPIO09 A12  - PULSE1_OUT    IO 6
GPIO10 A15  - DE_485
GPIO11 C7                   IO 7
GPIO12 B4                   IO 8
GPIO13 B5                   IO 9
GPIO14 B14                  IO 10
GPIO15 B15  - HPM_EN
GPIO16 D2                   IO 11
*/

#define IOS_STATE                                                      \
{                                                                      \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 0   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 1   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 2   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 3   */ \
    IO_AS_INPUT | IO_TYPE_PULSECOUNT | IO_TYPE_ONEWIRE, /* GPIO 4   */ \
    IO_AS_INPUT | IO_TYPE_PULSECOUNT | IO_TYPE_ONEWIRE, /* GPIO 5   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 6   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 7   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 8   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 9   */ \
}
