#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "lw.h"

#include "log.h"
#include "base_types.h"
#include "persist_config.h"
#include "common.h"

#define LW_PRINT_CFG_JSON_FMT_DEV_EUI                   "    \"DEV EUI\": \"%.*s\","
#define LW_PRINT_CFG_JSON_FMT_APP_KEY                   "    \"APP KEY\": \"%.*s\""

#define LW_PRINT_CFG_JSON_HEADER                        "{\n\r  \"type\": \"LW\",\n\r  \"config\": {"
#define LW_PRINT_CFG_JSON_DEV_EUI(_dev_eui)             LW_PRINT_CFG_JSON_FMT_DEV_EUI, LW_DEV_EUI_LEN, _dev_eui
#define LW_PRINT_CFG_JSON_APP_KEY(_app_key)             LW_PRINT_CFG_JSON_FMT_APP_KEY, LW_APP_KEY_LEN, _app_key
#define LW_PRINT_CFG_JSON_TAIL                          "  }\n\r}"


lw_config_t* lw_get_config(void)
{
    comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != COMMS_TYPE_LW)
    {
        comms_debug("Tried to get config for LORAWAN but config is not for LORAWAN.");
        return NULL;
    }
    return (lw_config_t*)comms_config;
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
    if (LW_CONFIG_VERSION != config->version)
    {
        return false;
    }
    for (uint8_t i = 0; i < LW_DEV_EUI_LEN; i++)
    {
        if (!isascii(config->dev_eui[i]) || config->dev_eui[i] == 0)
        {
            return false;
        }
    }
    for (uint8_t i = 0; i < LW_APP_KEY_LEN; i++)
    {
        if (!isascii(config->app_key[i]) || config->app_key[i] == 0)
        {
            return false;
        }
    }
    return config->region <= LW_REGION_MAX;
}


static const char* _lw_region_name(lw_region_t region)
{
    const char* name;
    switch (region)
    {
        case LW_REGION_EU433:
            name = LW_REGION_NAME_EU433;
            break;
        case LW_REGION_CN470:
            name = LW_REGION_NAME_CN470;
            break;
        case LW_REGION_RU864:
            name = LW_REGION_NAME_RU864;
            break;
        case LW_REGION_IN865:
            name = LW_REGION_NAME_IN865;
            break;
        case LW_REGION_EU868:
            name = LW_REGION_NAME_EU868;
            break;
        case LW_REGION_US915:
            name = LW_REGION_NAME_US915;
            break;
        case LW_REGION_AU915:
            name = LW_REGION_NAME_AU915;
            break;
        case LW_REGION_KR920:
            name = LW_REGION_NAME_KR920;
            break;
        case LW_REGION_AS923_1:
            name = LW_REGION_NAME_AS923_1;
            break;
        case LW_REGION_AS923_2:
            name = LW_REGION_NAME_AS923_2;
            break;
        case LW_REGION_AS923_3:
            name = LW_REGION_NAME_AS923_3;
            break;
        case LW_REGION_AS923_4:
            name = LW_REGION_NAME_AS923_4;
            break;
        default:
            name = NULL;
            break;
    }
    return name;
}


static bool _lw_region(char* name, unsigned len, lw_region_t* region)
{
    bool ret = true;
    if (len <= LW_REGION_LEN)
    {
        if (strlen(LW_REGION_NAME_EU433) == len &&
            strncmp(LW_REGION_NAME_EU433, name, len) == 0)
        {
            *region = LW_REGION_EU433;
        }
        else if (strlen(LW_REGION_NAME_CN470) == len &&
            strncmp(LW_REGION_NAME_CN470, name, len) == 0)
        {
            *region = LW_REGION_CN470;
        }
        else if (strlen(LW_REGION_NAME_RU864) == len &&
            strncmp(LW_REGION_NAME_RU864, name, len) == 0)
        {
            *region = LW_REGION_RU864;
        }
        else if (strlen(LW_REGION_NAME_IN865) == len &&
            strncmp(LW_REGION_NAME_IN865, name, len) == 0)
        {
            *region = LW_REGION_IN865;
        }
        else if (strlen(LW_REGION_NAME_EU868) == len &&
            strncmp(LW_REGION_NAME_EU868, name, len) == 0)
        {
            *region = LW_REGION_EU868;
        }
        else if (strlen(LW_REGION_NAME_US915) == len &&
            strncmp(LW_REGION_NAME_US915, name, len) == 0)
        {
            *region = LW_REGION_US915;
        }
        else if (strlen(LW_REGION_NAME_AU915) == len &&
            strncmp(LW_REGION_NAME_AU915, name, len) == 0)
        {
            *region = LW_REGION_AU915;
        }
        else if (strlen(LW_REGION_NAME_KR920) == len &&
            strncmp(LW_REGION_NAME_KR920, name, len) == 0)
        {
            *region = LW_REGION_KR920;
        }
        else if (strlen(LW_REGION_NAME_AS923_1) == len &&
            strncmp(LW_REGION_NAME_AS923_1, name, len) == 0)
        {
            *region = LW_REGION_AS923_1;
        }
        else if (strlen(LW_REGION_NAME_AS923_2) == len &&
            strncmp(LW_REGION_NAME_AS923_2, name, len) == 0)
        {
            *region = LW_REGION_AS923_2;
        }
        else if (strlen(LW_REGION_NAME_AS923_3) == len &&
            strncmp(LW_REGION_NAME_AS923_3, name, len) == 0)
        {
            *region = LW_REGION_AS923_3;
        }
        else if (strlen(LW_REGION_NAME_AS923_4) == len &&
            strncmp(LW_REGION_NAME_AS923_4, name, len) == 0)
        {
            *region = LW_REGION_AS923_4;
        }
        else
        {
            ret = false;
        }
    }
    else
    {
        log_out("HACK");
        ret = false;
    }
    return ret;
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
    if (strncmp(p, "region", wordlen) == 0)
    {
        /* Region */
        p = skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Region */
            if (!config || !lw_persist_data_is_valid())
            {
                log_out("Could not get region.");
                return false;
            }
            log_out("Region: %.*s (%"PRIu8")", LW_REGION_LEN, _lw_region_name(config->region), config->region);
            return false;
        }
        /* Set Region */
        lw_region_t region;
        if (!_lw_region(p, lenrem, &region))
        {
            log_out("Failed to find a region matching name %*.s", LW_REGION_LEN, p);
            return false;
        }
        config->region = (uint8_t)region;
        log_out("Set region to %.*s (%"PRIu8")", LW_REGION_LEN, p, config->region);
        return true;
    }
syntax_exit:
    log_out("lora_config dev-eui/app-key/region [EUI/KEY/REG]");
    return false;
}


uint64_t lw_consume(char *p, unsigned len)
{
    if (len > 16 || (len % 1))
        return 0;

    char tmp = p[len];
    p[len] = 0;
    uint64_t r = strtoul(p, NULL, 16);
    p[len] = tmp;
    return r;
}


/* Return true  if different
 *        false if same      */
bool lw_persist_config_cmp(lw_config_t* d0, lw_config_t* d1)
{
    return !(
        d0 && d1 &&
        memcmp(d0, d1, sizeof(lw_config_t)) == 0);
}


static void _lw_config_init2(lw_config_t* lw_config)
{
    lw_config->region = LW_REGION_EU868;
    lw_config->version = LW_CONFIG_VERSION;
    memset(lw_config->dev_eui, 0, LW_DEV_EUI_LEN);
    memset(lw_config->app_key, 0, LW_APP_KEY_LEN);
}


void lw_config_init(comms_config_t* comms_config)
{
    comms_config->type = COMMS_TYPE_LW;
    _lw_config_init2((lw_config_t*)comms_config);
}


void lw_print_config(void)
{
    const uint32_t loop_timeout = 10;
    log_out(LW_PRINT_CFG_JSON_HEADER);
    lw_config_t* config = lw_get_config();
    log_out_drain(loop_timeout);
    log_out(LW_PRINT_CFG_JSON_DEV_EUI(config->dev_eui));
    log_out_drain(loop_timeout);
    log_out(LW_PRINT_CFG_JSON_APP_KEY(config->app_key));
    log_out_drain(loop_timeout);
    log_out(LW_PRINT_CFG_JSON_TAIL);
    log_out_drain(loop_timeout);
}
