#include <string.h>
#include <ctype.h>

#include "lw.h"

#include "log.h"
#include "base_types.h"
#include "persist_config.h"


lw_config_t* lw_get_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
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


bool lw_persist_data_is_valid(void)
{
    lw_config_t* config = lw_get_config();
    for (uint8_t i = 0; i < LW_DEV_EUI_LEN; i++)
    {
        if (!isascii(config->dev_eui[i]) || config->dev_eui[i] == 0)
        {
            log_out("Dev EUI = %s", config->dev_eui);
            return false;
        }
    }
    for (uint8_t i = 0; i < LW_APP_KEY_LEN; i++)
    {
        if (!isascii(config->app_key[i]) || config->app_key[i] == 0)
        {
            log_out("App Key = %s", config->app_key);
            return false;
        }
    }
    return true;
}


bool lw_config_setup_str(char * str)
{
    // CMD  : "lora_config dev-eui 118f875d6994bbfd"
    // ARGS : "dev-eui 118f875d6994bbfd"
    char* p = skip_space(str);

    uint16_t lenrem = strnlen(p, CMD_LINELEN);
    if (lenrem == 0)
        goto syntax_exit;

    char* np = strchr(p, ' ');
    if (!np)
        np = p + lenrem;
    uint8_t wordlen = np - p;

    lw_config_t* config = lw_get_config();

    if (!config)
    {
        log_out("Could not get the LW config.");
        return false;
    }

    if (strncmp(p, "dev-eui", wordlen) == 0)
    {
        /* Dev EUI */
        p = skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Dev EUI */
            if (!config || !lw_persist_data_is_valid())
            {
                log_out("Could not get Dev EUI");
                return false;
            }
            log_out("Dev EUI: %."STR(LW_DEV_EUI_LEN)"s", config->dev_eui);
            return false;
        }
        /* Set Dev EUI */
        if (lenrem != LW_DEV_EUI_LEN)
        {
            log_out("Dev EUI should be %"PRIu16" characters long. (%"PRIu8")", LW_DEV_EUI_LEN, lenrem);
            return false;
        }
        memcpy(config->dev_eui, p, LW_DEV_EUI_LEN);
        return true;
    }
    if (strncmp(p, "app-key", wordlen) == 0)
    {
        /* App Key */
        p = skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Dev EUI */
            if (!config || !lw_persist_data_is_valid())
            {
                log_out("Could not get app key.");
                return false;
            }
            log_out("App Key: %."STR(LW_APP_KEY_LEN)"s", config->app_key);
            return false;
        }
        /* Set Dev EUI */
        if (lenrem != LW_APP_KEY_LEN)
        {
            log_out("App key should be %"PRIu16" characters long. (%"PRIu8")", LW_APP_KEY_LEN, lenrem);
            return false;
        }
        memcpy(config->app_key, p, LW_APP_KEY_LEN);
        return true;
    }
syntax_exit:
    log_out("lora_config dev-eui/app-key [EUI/KEY]");
    return false;
}
