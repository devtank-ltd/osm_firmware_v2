#pragma once

#include "model_config.h"

#include "rak3172.h"
#include "rak4270.h"

#define comms_get_mtu                                   CONCAT(comms_name,_get_mtu             )
#define comms_send_ready                                CONCAT(comms_name,_send_ready          )
#define comms_send_str                                  CONCAT(comms_name,_send_str            )
#define comms_send_allowed                              CONCAT(comms_name,_send_allowed        )
#define comms_send                                      CONCAT(comms_name,_send                )
#define comms_init                                      CONCAT(comms_name,_init                )
#define comms_reset                                     CONCAT(comms_name,_reset               )
#define comms_process                                   CONCAT(comms_name,_process             )
#define comms_get_connected                             CONCAT(comms_name,_get_connected       )
#define comms_loop_iteration                            CONCAT(comms_name,_loop_iteration      )
#define comms_config_setup_str                          CONCAT(comms_name,_config_setup_str    )
#define comms_get_id                                    CONCAT(comms_name,_get_id              )
#define comms_sent_ack                                  CONCAT(comms_name,_sent_ack            )
#define comms_add_commands                              CONCAT(comms_name,_add_commands        )
#define comms_power_down                                CONCAT(comms_name,_power_down          )

/* To be implemented by caller.*/
extern void     on_comms_sent_ack(bool acked) __attribute__((weak));
