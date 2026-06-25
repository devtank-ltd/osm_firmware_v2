Commands
========

You can issue commands via the USB port. It is a UART at 115200 baud rate and 8 bit characters, one stop bit and no parity (8N1).

Different models and ports have different commands, however, there is core set of commands.

Anything that has "Get/Set" will get with no further arguments, and set with the further argument if there is one.

          version : Print version.
            debug : Set hex debug mask
             name : Set/get serial number
            hw_id : Get Hardware ID
             save : Save config
            reset : Reset device.
             wipe : Factory Reset

The "version" command outputs something like:

    ============{
    Version : penguin_at_wifi-[3199]-14172cc8-Allow-stating-which-models-to-build
    }============

Which breaks down to: `<model>-[commit-num]-<commit-sha1>-<commit-comment>`

The "debug" command is to change the debug output.

The full list can be seen at [config.h](../../include/osm/core/config.h)

However, the most useful is 0x4 for Comms. There you can see why it's not connecting!

The number is taken as a hex, regardless of 0x prefix or not.

The system log mask 0x1, isn't optional. So you can do "debug 4" but you get 5.

Then "name" command is just the name reported in the devices payload.

For example, "name kettle", well mean kettle is reported. Just doing "name" will report back what the name is set to.

The "hw_id" command just reports the unique hex ID of the hardware.
The "save" command commits and changes to persistent storage.
The "wipe" command wipes that persistent storage.
The "reset" command just reboots the device.


The are core measurement related commands:

     measurements : Print measurements
      meas_enable : Enable measuremnts.
         get_meas : Get a measurement
      get_meas_to : Get timeout of measurement
    get_meas_type : Get the type of measurement
         interval : Get/Set the interval
      samplecount : Get/Set the samplecount
    interval_mins : Get/Set interval minutes
            repop : Repopulate measurements.
     is_immediate : Set/unset immediate measurements.

The "measurements" command lists all the measurements avaiable.
The "meas_enable" command just enables/disables collecting measurements.
The "get_meas" command gets the current value of the given measurement.
The "get_meas_to" command is the same but timesout on the "collection" time.
The "interval" command set the number of internvals between reporting the measurement.
The "samplecount" command sets the number of samples to take for the measurement between reporting.
The "interval_mins" command sets the numnber of minutes (as a fraction) a measurement interval is.
The "repop" command puts all the measurements back to the model default.
The "is_immediate" command changes if the measurement is read straight away. This is specially for pulses or GPIOs.


The are core comms related commands:
    
     comms_config : Set comms config
      j_comms_cfg : Print comms config
       comms_conn : Comms connected

The "comms_config" is a meta command and has it's own commands, different for different models. Which it will show you if you give it just "comms_config".

For example a WiFi OSM would give:

    ============{
     wifi_ssid : Set/get SSID
      wifi_pwd : Set/get password
       country : Set/get country
         schan : Set/get start channel
         nchan : Set/get number of channels
     mqtt_addr : Set/get MQTT address
     mqtt_user : Set/get MQTT user
      mqtt_pwd : Set/get MQTT password
      mqtt_sch : Set/get MQTT scheme
     mqtt_path : Set/get MQTT PATH
     mqtt_port : Set/get MQTT port
    }============

So for example to set the wifi:

    comms_config wifi_ssid my-wifi
    comms_config wifi_pwd my-wifi-password
    save

"mqtt_addr" is just host name or IP.
"mqtt_port" is just the host port to use for MQTT.
"mqtt_user" + "mqtt_pwd" is just username and password for MQTT auth.

"mqtt_sch" is a number of the scheme. Broadly it is plain TCP without SSL, with SSL, Websockets wtihout SSL or with SSL and differnet auths of SSL.

* 1  - MQTT over TCP.
* 2  - MQTT over TLS (no certificate verify).
* 3  - MQTT over TLS (verify server certificate).
* 4  - MQTT over TLS (provide client certificate).
* 5  - MQTT over TLS (verify server certificate and provide client certificate).
* 6  - MQTT over Websocket (TCP)
* 7  - MQTT over Websocket Secure (TLS no certificate verify).
* 8  - MQTT over Websocket Secure (TLS verify server certificate).
* 9  - MQTT over Websocket Secure (TLS provide client certificate).
* 10 - MQTT over Websocket Secure (TLS verify server certificate and provide client certificate).

"mqtt_path" is only for the Websocket option.


