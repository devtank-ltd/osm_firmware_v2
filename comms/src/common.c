#include <stddef.h>

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
