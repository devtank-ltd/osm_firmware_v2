#!/usr/bin/env python3

import os

import command_server

osm_loc = os.getenv("OSM_LOC", "/tmp/osm")
if osm_loc == "/tmp/osm":
    print("command_client_test.py: WARNING - If you wish to control a specific instance, please define OSM_LOC environment variable, otherwise default location(/tmp/osm will be used")
command_socket_file = osm_loc + "/command_socket"

def main():
    with command_server.command_command_client_t(command_socket_file) as cmd_clt:
        print("command_client_test.py: Connected to command socket")
        while True:
            try:
                prop = input("What data you wish to manipulate? Options:\nLGHT\nTEMP\nHUMI\nPM2.5\nPM10\n").upper()
                value = float(input("Value to manipulate: "))
                match prop:
                    case "LGHT":
                        print(f"Result: {cmd_clt.set_blocking("VEML7700", {"LUX": value})}")
                    case "TEMP":
                        print(f"Result: {cmd_clt.set_blocking("HTU21D", {"TEMP": value})}")
                    case "HUMI":
                        print(f"Result: {cmd_clt.set_blocking("HTU21D", {"HUMI": value})}")
                    # PM* not working
                    case "PM2.5":
                        print(f"Result: {cmd_clt.set_blocking("SEN5x", {"PM2_5": value})}")
                    case "PM10":
                        print(f"Result: {cmd_clt.set_blocking("SEN5x", {"PM10": value})}")
                    case _:
                        print("Unknown property")
            except KeyboardInterrupt:
                # exit properly
                sys.exit(0)

if __name__ == "__main__":
    import sys
    sys.exit(main())
