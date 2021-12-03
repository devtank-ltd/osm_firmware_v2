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

#define LW_ID__TEMPERATURE                  103     // C (we will use K)   | IPSO Alliance
#define LW_ID__HUMIDITY                     104     // % Relative Humidity | IPSO Alliance

#define LW_ID__ADDRESSABLE_TEXT_DISPLAY     141     //                     | IPSO Alliance

#define LW_ID__AIR_QUALITY                  228     //                     | uCIFI


#define LW_ID__PM10                         105
#define LW_ID__PM25                         106


#define LW__MAX_MEASUREMENTS                10

#define LW__DEV_EUI_LEN                     16
#define LW__APP_KEY_LEN                     32


typedef struct
{
    uint16_t    data_id;
    uint32_t    data;
    uint8_t     interval; // multiples of 5 mins
} lw_measurement_t;

typedef struct
{
    lw_measurement_t    measurements[LW__MAX_MEASUREMENTS];
    uint32_t            read_pos;
    uint32_t            write_pos;
} lw_packet_t;


extern void lorawan_init(void);
extern void lw_send(uint8_t* hex_arr, uint16_t arr_len);
extern void lw_send_str(char* str);
extern void lw_process(char* message);
extern bool lw_send_packet(lw_packet_t* packet, uint32_t interval_count);
