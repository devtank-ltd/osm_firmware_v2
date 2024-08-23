LoRaWAN Protocols
============

A custom JavaScript protocol is required to decode
data packets sent from the OSM.

There are several LoRaWAN applications available,
the main ones being ChirpStack, The Things Network
and Helium.

There are nuances between the custom protocols
for these applications. Therefore, we have
a script for each.

If you are using ChirpStack versions 3 and below, use cs_protocol.js

For The Things Network or ChirpStack version 4, use cs4_ttn_protocol.js

For Helium, use hl_protocol.js


Protocol layout
===============

| PROTOCOL | DATA NAME | DATA TYPE | VALUE TYPE |   DATA   |   DATA   |   DATA   | DATA NAME | DATA TYPE | VALUE TYPE | DATA |
|----------|-----------|-----------|------------|----------|----------|----------|-----------|-----------|------------|------|
|    02    |     T1    |   2 (avg) |   2 (u16)  | 533(avg) | 501(min) | 543(max) |     T2    | 1 (single)|  1 (uint8) |   1  |


Protocol
========
Version number of protocol. Currently 2.


Data Name
=========
Four characters of the name of the measurement.
For a list of non-configuration defined measurements names see  [measurements_mem.h](../../core/include/measurements_mem.h)

Data Type
=========
Can only be two values:

* 1 = Immediate measurement - single data point
* 2 = Averaged measurement - three data points, mean/avg, min and max.

Value Type
==========

    VALUE_TYPE_IS_SIGNED 0x10
        VALUE_UNSET   = 0,
        VALUE_UINT8   = 1,
        VALUE_UINT16  = 2,
        VALUE_UINT32  = 3,
        VALUE_UINT64  = 4,
        VALUE_INT8    = 1 | VALUE_TYPE_IS_SIGNED,
        VALUE_INT16   = 2 | VALUE_TYPE_IS_SIGNED,
        VALUE_INT32   = 3 | VALUE_TYPE_IS_SIGNED,
        VALUE_INT64   = 4 | VALUE_TYPE_IS_SIGNED,
        VALUE_FLOAT   = 5 | VALUE_TYPE_IS_SIGNED,
        VALUE_DOUBLE  = 6 | VALUE_TYPE_IS_SIGNED,

   Type tells you the payload length and type.
