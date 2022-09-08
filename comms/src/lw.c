#include <string.h>

#include "lw.h"

#include "log.h"
#include "base_types.h"
#include "persist_config.h"


lw_config_t* lw_get_config(void)
{
    comms_config_t* comms_config = persist_get_comms_config();
    if (comms_config->type != COMMS_TYPE_LW)
    {
        comms_debug("Tried to get config for LORAWAN but config is not for LORAWAN.");
        return NULL;
    }
    return (lw_config_t*)(comms_config->setup);
}


bool lw_get_id(char* str, uint8_t len)
{
    if (len < LW_DEV_EUI_LEN + 1)
        return false;
    lw_config_t* config = lw_get_config();
    if (!config)
        return false;
    strncpy(str, config->dev_eui, LW_DEV_EUI_LEN);
    str[LW_DEV_EUI_LEN] = 0;
    return true;
}
