#pragma once


typedef struct
{
    char        name[MEASURE_NAME_NULLED_LEN];
    float       coeffs[FTMA_NUM_COEFFS];
} ftma_config_t;


typedef ftma_config_t   adc_persist_config_t;
