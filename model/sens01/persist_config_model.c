#include "persist_config_header.h"
#include "ftma.h"


void persist_config_model_init(persist_model_config_t* model_config);
{
    model_config->mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
    ftma_setup_default_mem(&model_config->ftma_configs, sizeof(ftma_config_t));
}
