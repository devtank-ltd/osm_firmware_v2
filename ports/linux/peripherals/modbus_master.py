#!/usr/bin/env python3


import sys
import os
import time
from pymodbus.transaction import ModbusRtuFramer
from pymodbus.client import ModbusSerialClient
import logging


def main(args):

    FORMAT = ('%(asctime)-15s %(threadName)-15s'
              ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')
    logging.basicConfig(format=FORMAT)
    _log = logging.getLogger()

    if "DEBUG" in os.environ:
        _log.setLevel(logging.DEBUG)
    else:
        _log.setLevel(logging.CRITICAL)

    if len(args) > 1:
        dev = args[1]
    else:
        osm_loc = os.getenv("OSM_LOC", "/tmp/osm/")
        dev = os.path.join(osm_loc, "MODBUS_MASTER")
        if not os.path.exists(osm_loc):
            os.mkdir(osm_loc)

    logging.debug(f"Using {dev}")

    client = ModbusSerialClient(
        dev,
        framer=ModbusRtuFramer,
        baudrate=9600,
    )
    done = False
    while not done:
        try:
            res = client.read_holding_registers(0xc56e, 2, slave=0x5)
            logging.debug(res)
            if res.isError():
                logging.error(f"{res.exception_code = }")
            else:
                logging.debug(f"{res.registers = }")
            time.sleep(1)
        except KeyboardInterrupt:
            logging.info("Exiting...")
            done = True
    client.close()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
