#pragma once

#include <stdbool.h>

#define osm_comms_name                                      CONCAT(osm_,comms_name)

#define osm_comms_get_mtu                                   CONCAT(osm_comms_name,_get_mtu             )
#define osm_comms_send_ready                                CONCAT(osm_comms_name,_send_ready          )
#define osm_comms_send_str                                  CONCAT(osm_comms_name,_send_str            )
#define osm_comms_send_allowed                              CONCAT(osm_comms_name,_send_allowed        )
#define osm_comms_send                                      CONCAT(osm_comms_name,_send                )
#define osm_comms_init                                      CONCAT(osm_comms_name,_init                )
#define osm_comms_reset                                     CONCAT(osm_comms_name,_reset               )
#define osm_comms_process                                   CONCAT(osm_comms_name,_process             )
#define osm_comms_get_connected                             CONCAT(osm_comms_name,_get_connected       )
#define osm_comms_loop_iteration                            CONCAT(osm_comms_name,_loop_iteration      )
#define osm_comms_config_setup_str                          CONCAT(osm_comms_name,_config_setup_str    )
#define osm_comms_get_id                                    CONCAT(osm_comms_name,_get_id              )
#define osm_comms_sent_ack                                  CONCAT(osm_comms_name,_sent_ack            )
#define osm_comms_add_extra_commands                        CONCAT(osm_comms_name,_add_commands        )
#define osm_comms_power_down                                CONCAT(osm_comms_name,_power_down          )
#define osm_comms_persist_config_cmp                        CONCAT(osm_comms_name,_persist_config_cmp  )
#define osm_comms_get_unix_time                             CONCAT(osm_comms_name,_get_unix_time  )

#define osm_comms_cmd_config_cb                             CONCAT(osm_comms_name,_cmd_config_cb       )
#define osm_comms_cmd_j_cfg_cb                              CONCAT(osm_comms_name,_cmd_j_cfg_cb        )
#define osm_comms_cmd_conn_cb                               CONCAT(osm_comms_name,_cmd_conn_cb         )

#define COMMS_COMMON_JSON_OUT_STR(_m, _s, _l)                               \
    osm_cmd_ctx_out(ctx,_m(osm_comms_common_json_escape(_s, _l, '\\', "\"", 1)));   \
    osm_cmd_ctx_flush(ctx)

#define COMMS_COMMON_JSON_OUT_INT(_m, _i)                               \
    osm_cmd_ctx_out(ctx,_m(_i));                                            \
    osm_cmd_ctx_flush(ctx)


struct cmd_link_t* osm_comms_add_commands(struct cmd_link_t* tail);
char* osm_comms_common_json_escape(char* buf, unsigned bufsiz, const char escape_char, const char* escaped_char_list, const unsigned escaped_char_count);
