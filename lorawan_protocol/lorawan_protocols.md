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

If you are using ChirpStack, use cs_protocol.js

For The Things Network, use ttn_protocol.js

For Helium, use hl_protocol.js
