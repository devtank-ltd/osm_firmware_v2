#! /usr/bin/python3
import os
import sys
import json


ADCs={
    "name":"ADC",
    "PA4":0 ,
    "PA6":1 ,
    "PA7":2 ,
    "PB0":3 ,
    "PB1":4 ,
    "PC0":5 ,
    "PC1":6 ,
    "PC2":7 ,
    "PC3":8 ,
    "PC4":9 ,
    "PC5":10,
}

INPUTS={
    "name":"INPUT",
    "PC12": 0,
    "PA15": 1,
    "PB7" : 2,
    "PC13": 3,
    "PC14": 4,
    "PC15": 5,
    "PD2" : 6,
    "PF0" : 7,
    "PF1" : 8,
}

OUTPUTS={
    "name":"OUTPUT",
    "PC8"  : 0,
    "PC6"  : 1,
    "PB12" : 2,
    "PB11" : 3,
    "PB2"  : 4,
    "PB14" : 5,
    "PB15" : 6,
}

UARTS={
    "name":"UART",
    "PA2" : 1,
    "PA3" : 1,
    "PA9" : 2,
    "PA10" : 2,
    "PC10" : 3,
    "PC11" : 3,
    "PA1" : 4,
    "PA0" : 4,
}

PPS={
    "name":"PPS",
    "PB3" : 0,
    "PC7" : 1,
}


CN7={
    "name": "CN7",
    1 : "PC10",
    3 : "PC12",
    5 : "-",
    7 : "BOOT0",
    9 : "-",
    11 : "-",
    13 : "PA13",
    15 : "PA14",
    17 : "PA15",
    19 : "-",
    21 : "PB7",
    23 : "PC13",
    25 : "PC14",
    27 : "PC15",
    29 : "PF0",
    31 : "PF1",
    33 : "-",
    35 : "PC2",
    37 : "PC3",
    2 : "PC11",
    4 : "PD2",
    6 : "-",
    8 : "-",
    10 : "-",
    12 : "IOREF",
    14 : "RESET",
    16 : "-",
    18 : "-",
    20 : "-",
    22 : "-",
    24 : "-",
    26 : "-",
    28 : "PA0",
    30 : "PA1",
    32 : "PA4",
    34 : "PB0",
    36 : "PC1",
    38 : "PC0",
}

CN10={
    "name": "CN10",
    1 : "PC9"  ,
    3 : "PB8"  ,
    5 : "PB9"  ,
    7 : "-"    ,
    9 : "-"    ,
    11 : "PA5" ,
    13 : "PA6" ,
    15 : "PA7" ,
    17 : "PB6" ,
    19 : "PC7" ,
    21 : "PA9" ,
    23 : "PA8" ,
    25 : "PB10",
    27 : "PB4" ,
    29 : "PB5" ,
    31 : "PB3" ,
    33 : "PA10",
    35 : "PA2" ,
    37 : "PA3"  ,
    2 : "PC8"  ,
    4 : "PC6"  ,
    6 : "PC5"  ,
    8 : "-"    ,
    10 : "-"   ,
    12 : "PA12",
    14 : "PA11",
    16 : "PB12",
    18 : "PB11",
    20 : "-"   ,
    22 : "PB2" ,
    24 : "PB1" ,
    26 : "PB15",
    28 : "PB14",
    30 : "PB13",
    32 : "-"   ,
    34 : "PC4" ,
    36 : "-"   ,
    38 : "-"   ,
}


connectors = {"CN7" : CN7, "CN10": CN10}


def lookup_io_board(gpio_name):
    if gpio_name in ADCs:
        return ADCs
    if gpio_name in INPUTS:
        return INPUTS
    if gpio_name in OUTPUTS:
        return OUTPUTS
    if gpio_name in UARTS:
        return UARTS
    if gpio_name in PPS:
        return PPS


def print_io_board(gpio_name):
    role = lookup_io_board(gpio_name)
    if role:
        print("%s %u = %s" % (role["name"], role[gpio_name], gpio_name))


def lookup_connector(gpio_name, connector):
    connector_name = connector["name"]
    for p in connector:
        if connector[p] == gpio_name:
            print("%s %u = %s" % (connector_name, p, gpio_name))
            print_io_board(gpio_name)
            sys.exit(0)


def lookup_gpio(connector, pin):
    connector_name = connector["name"]
    gpio_name = connector[pin]
    print("%s %u = %s" % (connector_name, pin, gpio_name))
    print_io_board(gpio_name)
    sys.exit(0)


def print_help():
    print("2 arguments:")
    print("<connector> <pin>")
    print("connector = CN7 or CN10")
    print("pin = number")
    print("1 argument:")
    print("<GPIO Name>")


if len(sys.argv) < 2:
    print_help()
    sys.exit(-1)


if len(sys.argv) == 2:
    gpio_name = sys.argv[1]
    lookup_connector(gpio_name, CN7)
    lookup_connector(gpio_name, CN10)
    print("GPIO %s not found" % gpio_name)
    sys.exit(-1)

connector = sys.argv[1]
pin = int(sys.argv[2])

if connector not in connectors:
    print("%s is not CN7 or CN10" % connector)
    sys.exit(-1)

lookup_gpio(connectors[connector], pin)
