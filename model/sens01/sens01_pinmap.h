#pragma once


#define SENS01_ADCS_PORT_N_PINS                     \
{                                                   \
    {GPIOA, GPIO1},      /* ADC 1  = Channel 6  */  \
    {GPIOB, GPIO1},      /* ADC 1  = Channel 16 */  \
    {GPIOA, GPIO4},      /* ADC 1  = Channel 9  */  \
    {GPIOA, GPIO5},      /* ADC 1  = Channel 10 */  \
    {GPIOC, GPIO0},      /* ADC 1  = Channel 1  */  \
    {GPIOC, GPIO2},      /* ADC 1  = Channel 3  */  \
    {GPIOC, GPIO3},      /* ADC 1  = Channel 4  */  \
}

#define SENS01_ADC1_CHANNEL_FTMA_1                    6
#define SENS01_ADC1_CHANNEL_FTMA_2                    16
#define SENS01_ADC1_CHANNEL_FTMA_3                    9
#define SENS01_ADC1_CHANNEL_FTMA_4                    10
#define SENS01_ADC1_CHANNEL_BAT_MON            1
#define SENS01_ADC1_CHANNEL_3V3_RAIL_MON       3
#define SENS01_ADC1_CHANNEL_5V_RAIL_MON        4

#define SENS01_ADC_INDEX_FTMA_1            0
#define SENS01_ADC_INDEX_FTMA_2            1
#define SENS01_ADC_INDEX_FTMA_3            2
#define SENS01_ADC_INDEX_FTMA_4            3
#define SENS01_ADC_INDEX_BAT_MON           4
#define SENS01_ADC_INDEX_3V3_RAIL_MON      5
#define SENS01_ADC_INDEX_5V_RAIL_MON       6

#define SENS01_ADC_CHANNELS  { SENS01_ADC1_CHANNEL_FTMA_1,          \
                               SENS01_ADC1_CHANNEL_FTMA_2,          \
                               SENS01_ADC1_CHANNEL_FTMA_3,          \
                               SENS01_ADC1_CHANNEL_FTMA_4,          \
                               SENS01_ADC1_CHANNEL_BAT_MON,         \
                               SENS01_ADC1_CHANNEL_3V3_RAIL_MON,    \
                               SENS01_ADC1_CHANNEL_5V_RAIL_MON      }
#define ADC_FTMA_CHANNELS { SENS01_ADC1_CHANNEL_FTMA_1,  \
                            SENS01_ADC1_CHANNEL_FTMA_2,  \
                            SENS01_ADC1_CHANNEL_FTMA_3,  \
                            SENS01_ADC1_CHANNEL_FTMA_4   }

#define SENS01_ADC_DMA_CHANNELS                                                        \
{                                                                               \
    { ADC1, DMA1, RCC_DMA1, NVIC_DMA1_CHANNEL1_IRQ, DMA_CHANNEL1, ADC_PRIORITY  , true } , /* ADC1 */   \
}

#define SENS01_ADC_DMA_CHANNELS_COUNT  1

#define SENS01_ADC_COUNT       7
#define ADC_FTMA_COUNT  4


#define ADC_TYPES_ALL_FTMA { ADCS_TYPE_FTMA1,    \
                             ADCS_TYPE_FTMA2,    \
                             ADCS_TYPE_FTMA3,    \
                             ADCS_TYPE_FTMA4     }


#define CAN_PORT_N_PINS_RX    {GPIOB, GPIO12} /* CAN1RX */
#define CAN_PORT_N_PINS_TX    {GPIOB, GPIO13} /* CAN1TX */
#define CAN_PORT_N_PINS_STDBY {GPIOB, GPIO14} /* GPIO14 */


#define W1_PULSE_1_PORT_N_PINS      { GPIOA, GPIO11 }
#define W1_PULSE_2_PORT_N_PINS      { GPIOA, GPIO12 }
