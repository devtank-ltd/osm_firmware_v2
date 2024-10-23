#include <stddef.h>
#include <string.h>

#include "comms.h"
#include "common.h"
#include "cmd.h"
#include "base_types.h"
#include "model_config.h"


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


void comms_common_json_escape(char* buf, unsigned bufsiz, const char escape_char, const char escaped_char)
{
    unsigned len = strnlen(buf, bufsiz - 1);
    unsigned pos = 0;
    while (pos < len)
    {
        if (escaped_char == buf[pos])
        {
            for (unsigned j = len; j > pos; j--)
            {
                buf[j] = buf[j-1];
            }
            buf[pos] = escape_char;
            pos++;
            len++;
        }
        pos++;
    }
    buf[len] = 0;
}
