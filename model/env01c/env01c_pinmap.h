#pragma once


#define ENV01C_ADCS_PORT_N_PINS                      \
{                                                   \
    {GPIOA, GPIO1},      /* ADC 1  = Channel 6  */  \
    {GPIOB, GPIO1},      /* ADC 1  = Channel 16 */  \
    {GPIOA, GPIO4},      /* ADC 1  = Channel 9  */  \
    {GPIOC, GPIO0},      /* ADC 1  = Channel 1  */  \
    {GPIOC, GPIO1},      /* ADC 1  = Channel 2  */  \
    {GPIOC, GPIO2},      /* ADC 1  = Channel 3  */  \
    {GPIOC, GPIO3},      /* ADC 1  = Channel 4  */  \
}

#define ENV01C_ADC1_CHANNEL_CURRENT_CLAMP_1    6
#define ENV01C_ADC1_CHANNEL_CURRENT_CLAMP_2   16
#define ENV01C_ADC1_CHANNEL_CURRENT_CLAMP_3    9
#define ENV01C_ADC1_CHANNEL_BAT_MON            1
#define ENV01C_ADC1_CHANNEL_BAT_LVL_MON        2
#define ENV01C_ADC1_CHANNEL_3V3_RAIL_MON       3
#define ENV01C_ADC1_CHANNEL_5V_RAIL_MON        4

#define ENV01C_ADC_DMA_CHANNELS                                                  \
{                                                                               \
    { ADC1, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL1_IRQ, DMA_CHANNEL1, ADC_PRIORITY  , true } , /* ADC1 */   \
}

#define ENV01C_ADC_DMA_CHANNELS_COUNT  1

#define ENV01C_ADC_COUNT       7
#define ADC_CC_COUNT    3

#define CORE_3V3_EN_PORT_N_PINS     {GPIOB, GPIO3}


#define ADC_TYPES_ALL_CC { ADCS_TYPE_CC_CLAMP1,  \
                           ADCS_TYPE_CC_CLAMP2,  \
                           ADCS_TYPE_CC_CLAMP3   }


#define CAN_PORT_N_PINS_RX    {GPIOA, GPIO11} /* CAN1RX */
#define CAN_PORT_N_PINS_TX    {GPIOA, GPIO12} /* CAN1TX */
#define CAN_PORT_N_PINS_STDBY {GPIOB, GPIO14} /* GPIO14 */


#define W1_PULSE_1_PORT_N_PINS      { GPIOB, GPIO5 }
#define W1_PULSE_2_PORT_N_PINS      { GPIOC, GPIO11 }


#define IOS_PORT_N_PINS            \
{                                  \
    {GPIOC, GPIO6 },   /* IO 0  */ \
    {GPIOB, GPIO2 },   /* IO 1 */ \
    {GPIOC, GPIO9 },   /* IO 2  */ \
    {GPIOA, GPIO11 },  /* IO 4  */ \
    {GPIOA, GPIO12 },  /* IO 5  */ \
    {GPIOC, GPIO7 },   /* IO 6  */ \
    {GPIOB, GPIO4 },   /* IO 7  */ \
    {GPIOB, GPIO5 },   /* IO 8  */ \
    {GPIOD, GPIO2 },   /* IO 9 */ \
}


#define IOS_STATE                                                      \
{                                                                      \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 0   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 1   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 2   */ \
    IO_AS_INPUT | IO_TYPE_PULSECOUNT | IO_TYPE_ONEWIRE, /* GPIO 4   */ \
    IO_AS_INPUT | IO_TYPE_PULSECOUNT | IO_TYPE_ONEWIRE, /* GPIO 5   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 6   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 7   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 8   */ \
    IO_AS_INPUT | GPIO_PUPD_PULLDOWN,                   /* GPIO 9   */ \
}
