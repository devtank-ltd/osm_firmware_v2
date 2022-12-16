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

#define _3V3_CORE_EN_PORT_N_PINS     {GPIOB, GPIO3}


#define ADC_TYPES_ALL_CC { ADCS_TYPE_CC_CLAMP1,  \
                           ADCS_TYPE_CC_CLAMP2,  \
                           ADCS_TYPE_CC_CLAMP3   }
