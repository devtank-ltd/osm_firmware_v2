Volt Free Contact
=================

Some OSMS (RevG Env) have a volt free contact.

The spec is 24V 0.5A.

This is to be turned on/off remotely rather than by the OSM itself.

On Env RevG it is the IO 3.

So the command to turn it on, and make it a complete circuit is:

    IO 03 = ON

The command to turn it off is:

    IO 03 = OFF


With a PoE or WiFi OSM, to send this command remotely is done by sending to the MQTT topic 'osm/XXXXXXXX/cmd' where XXXXXXXX is the OSM hardware address.

An example of doing this with the mosquitto_pub tool is:

    mosquitto_pub -h some-host -u some-username -P some-password -t 'osm/XXXXXXXX/cmd' -m 'IO 03 = ON'
