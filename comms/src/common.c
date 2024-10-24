#include <stddef.h>
#include <string.h>

#include "comms.h"
#include "common.h"
#include "cmd.h"
#include "base_types.h"
#include "model_config.h"


#define COMMS_COMMON_BUF_SIZ                    128


static char comms_common_buf[COMMS_COMMON_BUF_SIZ];


struct cmd_link_t* comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_config", "Set comms config"        , comms_cmd_config_cb       , false , NULL },
        { "j_comms_cfg" , "Print comms config"      , comms_cmd_j_cfg_cb        , false , NULL },
        { "comms_conn"  , "Comms connected"         , comms_cmd_conn_cb         , false , NULL },
    };

    return comms_add_extra_commands(add_commands(tail, cmds, ARRAY_SIZE(cmds)));
}


char* comms_common_json_escape(char* buf, unsigned bufsiz, const char escape_char, const char* escaped_char_list, const unsigned escaped_char_count)
{
    unsigned len = strnlen(buf, bufsiz-1);
    char* p = comms_common_buf;
    for (unsigned i = 0; i < len; i++)
    {
        char c = buf[i];
        /* Remember to escape the escape character as well */
        if (escape_char == c)
        {
            *p++ = escape_char;
#define __COMMS_COMMON_JSON_CHECK_OVERFLOW                              \
            if (comms_common_buf + COMMS_COMMON_BUF_SIZ <= p)           \
            {                                                           \
                comms_common_buf[COMMS_COMMON_BUF_SIZ-1] = 0;           \
                return comms_common_buf;                                \
            }
            __COMMS_COMMON_JSON_CHECK_OVERFLOW
        }
        else
        {
            for (unsigned j = 0; j < escaped_char_count; j++)
            {
                if (escaped_char_list[j] == c)
                {
                    *p++ = escape_char;
                    __COMMS_COMMON_JSON_CHECK_OVERFLOW
                    break;
                }
            }
        }
        *p++ = c;
        __COMMS_COMMON_JSON_CHECK_OVERFLOW
    }
    *p = 0;
    return comms_common_buf;
}
