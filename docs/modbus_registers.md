Introduction
============

This is a quick document about the modbus setup for the currently
supported modbus smart meters. It could also be used as a reference on
how others should be setup up.

The units are reported as is read from the device and any processing of
the numbers is expected to happen else where.



SOCOMEC COUNTIS E53
===================

Short Name |   Full Name  |   Address     |   Type    |    Unit   |
-----------|--------------|---------------|-----------|-----------|
PF         | PowerFactor  |   0xc56e      |   U32     | - / 1000  |
cVP1       | Voltage P1   |   0xc552      |   U32     | V / 100   |
cVP2       | Voltage P2   |   0xc554      |   U32     | V / 100   |
cVP3       | Voltage P3   |   0xc556      |   U32     | V / 100   |
mAP1       | Current P1   |   0xc560      |   U32     | A / 1000  |
mAP2       | Current P2   |   0xc562      |   U32     | A / 1000  |
mAP3       | Current P3   |   0xc564      |   U32     | A / 1000  |
Imp        | Import       |   0xc652      |   U32     | Wh / 0.001|


Setup
-----

The device comms speed should be configured to:

* 9600 baudrate
* no parity
* 1 stop bit.

It should be configured to the Unit/Slave ID of 5.

The OSM can then be configured from it's terminal with:

    mb_setup RTU 9600 8N1
    mb_dev_add 5 E53

    mb_reg_add 5 0xc56e 3 U32 PF
    mb_reg_add 5 0xc552 3 U32 cVP1
    mb_reg_add 5 0xc554 3 U32 cVP2
    mb_reg_add 5 0xc556 3 U32 cVP3
    mb_reg_add 5 0xc560 3 U32 mAP1
    mb_reg_add 5 0xc562 3 U32 mAP2
    mb_reg_add 5 0xc564 3 U32 mAP3
    mb_reg_add 5 0xc652 3 U32 Imp


RI-F220
=======

Short Name |  Full Name   |    Address    |   Type    |    Unit   |
-----------|--------------|---------------|-----------|-----------|
PF         | Average PF   |     0x36      |   F       |     1     |
VP1        | Voltage V1N  |     0x00      |   F       |     V     |
VP2        | Voltage V2N  |     0x02      |   F       |     V     |
VP3        | Voltage V3N  |     0x04      |   F       |     V     |
AP1        | Current I1   |     0x10      |   F       |     A     |
AP2        | Current I2   |     0x12      |   F       |     A     |
AP3        | Current I3   |     0x14      |   F       |     A     |
Imp        | Total IMP kWh|     0x60      |   F       |     kWh   |


Setup
-----

The device comms speed should be configured to:

* 9600 baudrate
* no parity
* 1 stop bit.

It should be configured to the Unit/Slave ID of 1.

The OSM can then be configured from it's terminal with:


    mb_setup RTU 9600 8N1
    mb_dev_add 1 RIF

    mb_reg_add 1 0x36 4 F PF
    mb_reg_add 1 0x00 4 F VP1
    mb_reg_add 1 0x02 4 F VP2
    mb_reg_add 1 0x04 4 F VP3
    mb_reg_add 1 0x10 4 F AP1
    mb_reg_add 1 0x12 4 F AP2
    mb_reg_add 1 0x14 4 F AP3
    mb_reg_add 1 0x60 4 F Imp
