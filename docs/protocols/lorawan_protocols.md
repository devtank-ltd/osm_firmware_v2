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

       x2    ||      x8       ||    x2     ||     x2     ||   xX   ||     x2     ||   xX   ||     x2     ||   xX   ||      x8       ||    x2     ||     x2     ||   xX   ||     x2     ||   xX   ||     x2     ||   x4   ||      x8       ||    x2     ||     x2     ||   xX   ||     x2     ||   xX   ||     x2     ||   x4   ||
             ||               ||           ||            ||        ||            ||        ||            ||        ||               ||           ||            ||        ||            ||        ||            ||        ||               ||           ||            ||        ||            ||        ||            ||        ||
    PROTOCOL ||   DATA NAME   || DATA TYPE || VALUE TYPE ||  DATA  || VALUE TYPE ||  DATA  || VALUE TYPE ||  DATA  ||   DATA NAME   || DATA TYPE || VALUE TYPE ||  DATA  || VALUE TYPE ||  DATA  || VALUE TYPE ||  DATA  ||   DATA NAME   || DATA TYPE || VALUE TYPE ||  DATA  || VALUE TYPE ||  DATA  || VALUE TYPE ||  DATA  ||
             ||               ||           ||            ||        ||            ||        ||            ||        ||               ||           ||            ||        ||            ||        ||            ||        ||               ||           ||            ||        ||            ||        ||            ||        ||
       01    ||   43433100    ||    01     ||            ||  0C11  ||            ||  0C11  ||            ||  0C11  ||   544d5031    ||    02     ||            ||  0C11  ||            ||  0C11  ||            ||  0C11  ||   544d5031    ||    02     ||            ||  0C11  ||            ||  0C11  ||            ||  0C11  ||


       01        504d3130           02             01         00         02          0000         02         0000       504d3235          02             01         00          02         0000          02        0000       43433100          02            02         5418         02         0000         02         b21b    00

       01        504d3130           02             01         00         02          0000         02         0000       504d3235          02             01         00          02         0000          02        0000       43433100          02            02         ae31         02         0000         02         0000    00


Protocol
========
Version number of protocol. Currently 2.


Data Name
=========
For a list of non-configuration defined measurements names see  [measurements_mem.h](../../core/include/measurements_mem.h)

Data Type
=========
   Immediate measurement (1) or averaged measurement (2).

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
