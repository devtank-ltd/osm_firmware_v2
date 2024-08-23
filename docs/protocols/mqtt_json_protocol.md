JSON over MQTT
==============

Devtank's wifi protocol is a simple JSON payload over MQTT.

An example of the payload is:


    {
      "UNIX" : 1719840134,
      "NAME" : "Kettle1 Mon",
      "VALUES":
      {
         "TEMP" : "23.5",
         "TEMP_min" : "23",
         "TEMP_max" : "24",
         "CC1" : "15.5",
         "CC1_min" : "15.4",
         "CC1_max" : "15.6"
      }
    }


Not all measurement readings have a min and a max. Only averaged readings have this.

The schema for the payload is :


    {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "type": "object",
        "properties": {
            "NAME": {
                "type": "string",
                "$comment" : "Name given to sensor in config."
            },
            "UNIX": {
                "type": "integer",
                "$comment" : "Unix timestamp."
            },
            "VALUES": {
                "type": "object",
                "$comment" : "Map of measurements values",
                "properties": {
                    "int_measurement" : { "type": "integer", "$comment" : "Average reading or single reading" },
                    "int_measurement_min" : { "type": "integer", "$comment" : "Minimum from readings" },
                    "int_measurement_max" : { "type": "integer", "$comment" : "Maximum from readings" },
                    "str_measurement" : { "type": "string" },
                    "float_measurement" : { "type": "number", "$comment" : "Average reading or single reading" },
                    "float_measurement_min" : { "type": "integer", "$comment" : "Minimum from readings" },
                    "float_measurement_max" : { "type": "integer", "$comment" : "Maximum from readings" },
                }
            }
        },
    
        "required": ["NAME", "UNIX", "VALUES"]
    } 


For a list of non-configuration defined measurements see  [measurements_mem.h](../../core/include/measurements_mem.h)

However modbus measurement names are down to the configuration.
