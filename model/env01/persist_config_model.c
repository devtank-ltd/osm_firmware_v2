#include "persist_config_header.h"
#include "cc.h"


void persist_config_model_init(persist_model_config_t* model_config)
{
    model_config->mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
    cc_setup_default_mem(model_config->cc_configs, sizeof(cc_config_t));
}
