#pragma once

#include <stdbool.h>


/** IDs are taken from or abide by similar rules to the Cayenne LPP.
 *  https://devtoolkit.openmobilealliance.org/OEditor/default.aspx
 *  
 *  IDs = OMA Object ID - 3200
 *  as defined by Cayenne Protocol.
 *  https://developers.mydevices.com/cayenne/docs/lora/#lora-cayenne-low-power-payload
 *                                                     EXAMPLE UNITS         OWNER
 */
#define LW_ID__DIGITAL_INPUT                0       //                     | IPSO Alliance
#define LW_ID__DIGITAL_OUTPUT               1       //                     | IPSO Alliance
#define LW_ID__ANALOG_INPUT                 2       //                     | IPSO Alliance
#define LW_ID__ANALOG_OUTPUT                3       //                     | IPSO Alliance


#define LW_ID__GENERAL_SENSOR               100     //                     | IPSO Alliance
#define LW_ID__ILLUMINANCE                  101     // lux                 | IPSO Alliance
#define LW_ID__PRESENCE                     102     //                     | IPSO Alliance
#define LW_ID__TEMPERATURE                  103     // C (we will use K)   | IPSO Alliance
#define LW_ID__HUMIDITY                     104     // % Relative Humidity | IPSO Alliance
#define LW_ID__POWER_MEASUREMENT            105     // (Power factor?)     | IPSO Alliance
#define LW_ID__ACTUATION                    106     //                     | IPSO Alliance

#define LW_ID__SET_POINT                    108     //                     | IPSO Alliance

#define LW_ID__LOAD_CONTROL                 110     //                     | IPSO Alliance
#define LW_ID__LIGHT_CONTROL                111     //                     | IPSO Alliance
#define LW_ID__POWER_CONTROL                112     //                     | IPSO Alliance
#define LW_ID__ACCELEROMETER                113     //                     | IPSO Alliance
#define LW_ID__MAGNETOMETER                 114     //                     | IPSO Alliance
#define LW_ID__BAROMETER                    115     // Pa                  | IPSO Alliance
#define LW_ID__VOLTAGE                      116     // V                   | IPSO Alliance
#define LW_ID__CURRENT                      117     // A                   | IPSO Alliance
#define LW_ID__FREQUENCY                    118     // Hz                  | IPSO Alliance
#define LW_ID__DEPTH                        119     // mm (rain)           | IPSO Alliance
#define LW_ID__PERCENTAGE                   120     // % (fill of vessel)  | IPSO Alliance
#define LW_ID__ALTITUDE                     121     // m                   | IPSO Alliance
#define LW_ID__LOAD                         122     // kg                  | IPSO Alliance
#define LW_ID__PRESSURE                     123     // Pa                  | IPSO Alliance
#define LW_ID__LOUDNESS                     124     // dB                  | IPSO Alliance
#define LW_ID__CONCENTRATION                125     // ppm                 | IPSO Alliance
#define LW_ID__ACIDITY                      126     // pH                  | IPSO Alliance
#define LW_ID__CONDUCTIVITY                 127     // S                   | IPSO Alliance
#define LW_ID__POWER                        128     // W                   | IPSO Alliance
#define LW_ID__POWER_FACTOR                 129     //                     | IPSO Alliance
#define LW_ID__DISTANCE                     130     // m                   | IPSO Alliance
#define LW_ID__ENERGY                       131     // Wh                  | IPSO Alliance
#define LW_ID__DIRECTION                    132     // deg                 | IPSO Alliance
#define LW_ID__TIME                         133     // s Since Epoch       | IPSO Alliance
#define LW_ID__GYROMETER                    134     // Rad/s               | IPSO Alliance
#define LW_ID__COLOUR                       135     //                     | IPSO Alliance
#define LW_ID__LOCATION                     136     //                     | IPSO Alliance
#define LW_ID__POSITIONER                   137     // %                   | IPSO Alliance
#define LW_ID__BUZZER                       138     //                     | IPSO Alliance
#define LW_ID__AUDIO_CLIP                   139     //                     | IPSO Alliance
#define LW_ID__TIMER                        140     //                     | IPSO Alliance
#define LW_ID__ADDRESSABLE_TEXT_DISPLAY     141     //                     | IPSO Alliance
#define LW_ID__ON_OFF_SWITCH                142     //                     | IPSO Alliance
#define LW_ID__DIMMER                       143     //                     | IPSO Alliance
#define LW_ID__UP_DOWN_CONTROL              144     //                     | IPSO Alliance


#define LW_ID__GAS_METER                    223     // m3                  | uCIFI
#define LW_ID__WATER_METER                  224     // m3                  | uCIFI

#define LW_ID__WATER_QUALITY_SENSOR         226     //                     | uCIFI

#define LW_ID__AIR_QUALITY                  228     //                     | uCIFI


#define LW__MAX_MEASUREMENTS                128


typedef struct
{
    uint8_t     data_id;
    uint32_t    data;
} lw_measurement_t;

typedef struct
{
    lw_measurement_t    measurements[LW__MAX_MEASUREMENTS];
    uint32_t            read_pos;
    uint32_t            write_pos;
} lw_packet_t;


extern void lorawan_init(void);
extern void lw_main(void);
extern void lw_process(char* message);
