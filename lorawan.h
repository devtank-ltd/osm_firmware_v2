#pragma once

#include <stdbool.h>
#include <inttypes.h>


/** IDs are taken from or abide by similar rules to the Cayenne LPP.
 *  https://devtoolkit.openmobilealliance.org/OEditor/default.aspx
 *
 *  IDs = OMA Object ID - 3200
 *  as defined by Cayenne Protocol.
 *  https://developers.mydevices.com/cayenne/docs/lora/#lora-cayenne-low-power-payload
 *                                                     EXAMPLE UNITS         OWNER
 */

#define LW_DEV_EUI_LEN                     16
#define LW_APP_KEY_LEN                     32

#define LW_HEADER_SIZE          17
#define LW_TAIL_SIZE             2


extern void lorawan_init(void);
extern void lw_send(int8_t* hex_arr, uint16_t arr_len);
extern void lw_send_str(char* str);
extern unsigned lw_send_size(uint16_t arr_len);
extern bool lw_send_ready(void);
extern void lw_reconnect(void);
extern void lw_process(char* message);
extern bool lw_get_connected(void);
extern void lw_send_alive(void);
extern void lw_loop_iteration(void);

/* To be implemented by caller.*/
extern void on_lw_sent_ack(bool acked) __attribute__((weak));

