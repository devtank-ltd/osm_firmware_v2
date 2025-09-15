#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <osm/comms/lw.h>

#include <osm/core/log.h>
#include <osm/core/base_types.h>
#include <osm/core/persist_config.h>
#include <osm/core/common.h>

#define LW_PRINT_CFG_JSON_FMT_DEV_EUI                   "    \"DEV EUI\": \"%.*s\","
#define LW_PRINT_CFG_JSON_FMT_APP_KEY                   "    \"APP KEY\": \"%.*s\""

#define LW_PRINT_CFG_JSON_HEADER                        "{\n\r  \"type\": \"LW\",\n\r  \"config\": {"
#define LW_PRINT_CFG_JSON_DEV_EUI(_dev_eui)             LW_PRINT_CFG_JSON_FMT_DEV_EUI, OSM_LW_DEV_EUI_LEN, _dev_eui
#define LW_PRINT_CFG_JSON_APP_KEY(_app_key)             LW_PRINT_CFG_JSON_FMT_APP_KEY, OSM_LW_APP_KEY_LEN, _app_key
#define LW_PRINT_CFG_JSON_TAIL                          "  }\n\r}"


osm_lw_config_t* osm_lw_get_config(void)
{
    osm_comms_config_t* comms_config = &persist_data.model_config.comms_config;
    if (comms_config->type != OSM_COMMS_TYPE_LW)
    {
        osm_comms_debug("Tried to get config for LORAWAN but config is not for LORAWAN.");
        return NULL;
    }
    return (osm_lw_config_t*)comms_config;
}


bool osm_lw_get_id(char* str, uint8_t len)
{
    if (len < OSM_LW_DEV_EUI_LEN + 1)
        return false;
    osm_lw_config_t* config = osm_lw_get_config();
    if (!config)
        return false;
    strncpy(str, config->dev_eui, OSM_LW_DEV_EUI_LEN);
    str[OSM_LW_DEV_EUI_LEN] = 0;
    return true;
}


bool osm_lw_persist_data_is_valid(void)
{
    osm_lw_config_t* config = osm_lw_get_config();
    if (OSM_LW_CONFIG_VERSION != config->version)
    {
        return false;
    }
    for (uint8_t i = 0; i < OSM_LW_DEV_EUI_LEN; i++)
    {
        if (!isascii(config->dev_eui[i]) || config->dev_eui[i] == 0)
        {
            return false;
        }
    }
    for (uint8_t i = 0; i < OSM_LW_APP_KEY_LEN; i++)
    {
        if (!isascii(config->app_key[i]) || config->app_key[i] == 0)
        {
            return false;
        }
    }
    return config->region <= OSM_LW_REGION_MAX;
}


static const char* _lw_region_name(osm_lw_region_t region)
{
    const char* name;
    switch (region)
    {
        case OSM_LW_REGION_EU433:
            name = OSM_LW_REGION_NAME_EU433;
            break;
        case OSM_LW_REGION_CN470:
            name = OSM_LW_REGION_NAME_CN470;
            break;
        case OSM_LW_REGION_RU864:
            name = OSM_LW_REGION_NAME_RU864;
            break;
        case OSM_LW_REGION_IN865:
            name = OSM_LW_REGION_NAME_IN865;
            break;
        case OSM_LW_REGION_EU868:
            name = OSM_LW_REGION_NAME_EU868;
            break;
        case OSM_LW_REGION_US915:
            name = OSM_LW_REGION_NAME_US915;
            break;
        case OSM_LW_REGION_AU915:
            name = OSM_LW_REGION_NAME_AU915;
            break;
        case OSM_LW_REGION_KR920:
            name = OSM_LW_REGION_NAME_KR920;
            break;
        case OSM_LW_REGION_AS923_1:
            name = OSM_LW_REGION_NAME_AS923_1;
            break;
        case OSM_LW_REGION_AS923_2:
            name = OSM_LW_REGION_NAME_AS923_2;
            break;
        case OSM_LW_REGION_AS923_3:
            name = OSM_LW_REGION_NAME_AS923_3;
            break;
        case OSM_LW_REGION_AS923_4:
            name = OSM_LW_REGION_NAME_AS923_4;
            break;
        default:
            name = NULL;
            break;
    }
    return name;
}


static bool _lw_region(char* name, unsigned len, osm_lw_region_t* region)
{
    bool ret = true;
    if (len <= OSM_LW_REGION_LEN)
    {
        if (strlen(OSM_LW_REGION_NAME_EU433) == len &&
            strncmp(OSM_LW_REGION_NAME_EU433, name, len) == 0)
        {
            *region = OSM_LW_REGION_EU433;
        }
        else if (strlen(OSM_LW_REGION_NAME_CN470) == len &&
            strncmp(OSM_LW_REGION_NAME_CN470, name, len) == 0)
        {
            *region = OSM_LW_REGION_CN470;
        }
        else if (strlen(OSM_LW_REGION_NAME_RU864) == len &&
            strncmp(OSM_LW_REGION_NAME_RU864, name, len) == 0)
        {
            *region = OSM_LW_REGION_RU864;
        }
        else if (strlen(OSM_LW_REGION_NAME_IN865) == len &&
            strncmp(OSM_LW_REGION_NAME_IN865, name, len) == 0)
        {
            *region = OSM_LW_REGION_IN865;
        }
        else if (strlen(OSM_LW_REGION_NAME_EU868) == len &&
            strncmp(OSM_LW_REGION_NAME_EU868, name, len) == 0)
        {
            *region = OSM_LW_REGION_EU868;
        }
        else if (strlen(OSM_LW_REGION_NAME_US915) == len &&
            strncmp(OSM_LW_REGION_NAME_US915, name, len) == 0)
        {
            *region = OSM_LW_REGION_US915;
        }
        else if (strlen(OSM_LW_REGION_NAME_AU915) == len &&
            strncmp(OSM_LW_REGION_NAME_AU915, name, len) == 0)
        {
            *region = OSM_LW_REGION_AU915;
        }
        else if (strlen(OSM_LW_REGION_NAME_KR920) == len &&
            strncmp(OSM_LW_REGION_NAME_KR920, name, len) == 0)
        {
            *region = OSM_LW_REGION_KR920;
        }
        else if (strlen(OSM_LW_REGION_NAME_AS923_1) == len &&
            strncmp(OSM_LW_REGION_NAME_AS923_1, name, len) == 0)
        {
            *region = OSM_LW_REGION_AS923_1;
        }
        else if (strlen(OSM_LW_REGION_NAME_AS923_2) == len &&
            strncmp(OSM_LW_REGION_NAME_AS923_2, name, len) == 0)
        {
            *region = OSM_LW_REGION_AS923_2;
        }
        else if (strlen(OSM_LW_REGION_NAME_AS923_3) == len &&
            strncmp(OSM_LW_REGION_NAME_AS923_3, name, len) == 0)
        {
            *region = OSM_LW_REGION_AS923_3;
        }
        else if (strlen(OSM_LW_REGION_NAME_AS923_4) == len &&
            strncmp(OSM_LW_REGION_NAME_AS923_4, name, len) == 0)
        {
            *region = OSM_LW_REGION_AS923_4;
        }
        else
        {
            ret = false;
        }
    }
    else
    {
        osm_comms_debug("HACK");
        ret = false;
    }
    return ret;
}


bool osm_lw_config_setup_str(char * str, osm_cmd_ctx_t * ctx)
{
    // CMD  : "lora_config dev-eui 118f875d6994bbfd"
    // ARGS : "dev-eui 118f875d6994bbfd"
    char* p = osm_skip_space(str);

    uint16_t lenrem = strnlen(p, CMD_LINELEN);
    if (lenrem == 0)
        goto syntax_exit;

    char* np = strchr(p, ' ');
    if (!np)
        np = p + lenrem;
    uint8_t wordlen = np - p;

    osm_lw_config_t* config = osm_lw_get_config();

    if (!config)
    {
        osm_cmd_ctx_out(ctx,"Could not get the LW config.");
        return false;
    }

    if (strncmp(p, "dev-eui", wordlen) == 0)
    {
        /* Dev EUI */
        p = osm_skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Dev EUI */
            if (!config || !osm_lw_persist_data_is_valid())
            {
                osm_cmd_ctx_error(ctx,"Could not get Dev EUI");
                return false;
            }
            osm_cmd_ctx_out(ctx,"Dev EUI: %."STR(OSM_LW_DEV_EUI_LEN)"s", config->dev_eui);
            return false;
        }
        /* Set Dev EUI */
        if (lenrem != OSM_LW_DEV_EUI_LEN)
        {
            osm_cmd_ctx_out(ctx,"Dev EUI should be %"PRIu16" characters long. (%"PRIu8")", OSM_LW_DEV_EUI_LEN, lenrem);
            return false;
        }
        memcpy(config->dev_eui, p, OSM_LW_DEV_EUI_LEN);
        return true;
    }
    if (strncmp(p, "app-key", wordlen) == 0)
    {
        /* App Key */
        p = osm_skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Dev EUI */
            if (!config || !osm_lw_persist_data_is_valid())
            {
                osm_cmd_ctx_error(ctx,"Could not get app key.");
                return false;
            }
            osm_cmd_ctx_out(ctx,"App Key: %."STR(OSM_LW_APP_KEY_LEN)"s", config->app_key);
            return false;
        }
        /* Set Dev EUI */
        if (lenrem != OSM_LW_APP_KEY_LEN)
        {
            osm_cmd_ctx_out(ctx,"App key should be %"PRIu16" characters long. (%"PRIu8")", OSM_LW_APP_KEY_LEN, lenrem);
            return false;
        }
        memcpy(config->app_key, p, OSM_LW_APP_KEY_LEN);
        return true;
    }
    if (strncmp(p, "region", wordlen) == 0)
    {
        /* Region */
        p = osm_skip_space(np);
        lenrem = strnlen(p, CMD_LINELEN);
        if (lenrem == 0)
        {
            /* View Region */
            if (!config || !osm_lw_persist_data_is_valid())
            {
                osm_cmd_ctx_error(ctx,"Could not get region.");
                return false;
            }
            osm_cmd_ctx_out(ctx,"Region: %.*s (%"PRIu8")", OSM_LW_REGION_LEN, _lw_region_name(config->region), config->region);
            return false;
        }
        /* Set Region */
        osm_lw_region_t region;
        if (!_lw_region(p, lenrem, &region))
        {
            osm_cmd_ctx_error(ctx,"Failed to find a region matching name %*.s", OSM_LW_REGION_LEN, p);
            return false;
        }
        config->region = (uint8_t)region;
        osm_cmd_ctx_out(ctx,"Set region to %.*s (%"PRIu8")", OSM_LW_REGION_LEN, p, config->region);
        return true;
    }
syntax_exit:
    osm_cmd_ctx_out(ctx,"lora_config dev-eui/app-key/region [EUI/KEY/REG]");
    return false;
}


uint64_t osm_lw_consume(char *p, unsigned len)
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
bool osm_lw_persist_config_cmp(osm_lw_config_t* d0, osm_lw_config_t* d1)
{
    return !(
        d0 && d1 &&
        memcmp(d0, d1, sizeof(osm_lw_config_t)) == 0);
}


static void _lw_config_init2(osm_lw_config_t* lw_config)
{
    lw_config->region = OSM_LW_REGION_EU868;
    lw_config->version = OSM_LW_CONFIG_VERSION;
    memset(lw_config->dev_eui, 0, OSM_LW_DEV_EUI_LEN);
    memset(lw_config->app_key, 0, OSM_LW_APP_KEY_LEN);
}


void osm_lw_config_init(osm_comms_config_t* comms_config)
{
    comms_config->type = OSM_COMMS_TYPE_LW;
    _lw_config_init2((osm_lw_config_t*)comms_config);
}


void osm_lw_print_config(osm_cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,LW_PRINT_CFG_JSON_HEADER);
    osm_lw_config_t* config = osm_lw_get_config();
    osm_cmd_ctx_flush(ctx);
    osm_cmd_ctx_out(ctx,LW_PRINT_CFG_JSON_DEV_EUI(config->dev_eui));
    osm_cmd_ctx_flush(ctx);
    osm_cmd_ctx_out(ctx,LW_PRINT_CFG_JSON_APP_KEY(config->app_key));
    osm_cmd_ctx_flush(ctx);
    osm_cmd_ctx_out(ctx,LW_PRINT_CFG_JSON_TAIL);
    osm_cmd_ctx_flush(ctx);
}
