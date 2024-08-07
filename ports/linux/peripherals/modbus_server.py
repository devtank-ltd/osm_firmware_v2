#!/usr/bin/env python3
import os
import logging
from pymodbus.payload import BinaryPayloadBuilder
from pymodbus.transaction import ModbusRtuFramer
try:
    from pymodbus.version import version
except ModuleNotFoundError:
    import pymodbus
    version_short = pymodbus.__version__
    version_major = int(version_short.split(".")[0])
else:
    version_major = version.major
    version_short = version.short
if version_major < 3:
    from pymodbus.server.sync import StartSerialServer
else:
    from pymodbus.server import StartSerialServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext, ModbusSparseDataBlock
from pymodbus.constants import Endian

try:
    endian_big = Endian.Big
    endian_little = Endian.Little
except AttributeError:
    endian_big = Endian.BIG
    endian_little = Endian.LITTLE


MODBUS_DEV_ADDRESS_E53  = 0x5
MODBUS_REGISTERS_E53    = {0xc56e: (BinaryPayloadBuilder.add_32bit_uint,  1000),  # PF    "PowerFactorP1",  "PF"
                           0xc552: (BinaryPayloadBuilder.add_32bit_uint, 24001),  # cVP1  "VoltageP1",      "V / 100"         , 24000 for 240 in centivolts
                           0xc554: (BinaryPayloadBuilder.add_32bit_uint, 24002),  # cVP2  "VoltageP2",      "V / 100"         ,
                           0xc556: (BinaryPayloadBuilder.add_32bit_uint, 24003),  # cVP3  "VoltageP3",      "V / 100"         ,
                           0xc560: (BinaryPayloadBuilder.add_32bit_uint,  3001),  # mAP1  "CurrentP1",      "A / 1000"        , 3000 for 3A in milliamps
                           0xc562: (BinaryPayloadBuilder.add_32bit_uint,  3002),  # mAP2  "CurrentP2",      "A / 1000"        ,
                           0xc564: (BinaryPayloadBuilder.add_32bit_uint,  3003),  # mAP3  "CurrentP3",      "A / 1000"        ,
                           0xc652: (BinaryPayloadBuilder.add_32bit_uint,  1000)   # ImEn  "ImportEnergy",   "watt/hours/0.001", 1000 for 1.0
                           }

MODBUS_DEV_ADDRESS_RIF  = 0x1
MODBUS_REGISTERS_RIF    = {0x36: (BinaryPayloadBuilder.add_32bit_float, 1. ),   # PF   "PowerFactorP1",  "PF"
                           0x00: (BinaryPayloadBuilder.add_32bit_float, 240.1), # VP1  "VoltageP1",      "V"
                           0x02: (BinaryPayloadBuilder.add_32bit_float, 240.2), # VP2  "VoltageP2",      "V"
                           0x04: (BinaryPayloadBuilder.add_32bit_float, 240.3), # VP3  "VoltageP3",      "V"
                           0x10: (BinaryPayloadBuilder.add_32bit_float, 30.1),  # AP1  "CurrentP1",      "A"
                           0x12: (BinaryPayloadBuilder.add_32bit_float, 30.2),  # AP2  "CurrentP2",      "A"
                           0x14: (BinaryPayloadBuilder.add_32bit_float, 30.3),  # AP3  "CurrentP3",      "A"
                           0x60: (BinaryPayloadBuilder.add_32bit_float, 1.)     # Imp  "ImportEnergy",   "watt/hours/1"
                           }
MODBUS_DEV_ADDRESS_RDL  = 0x2
MODBUS_REGISTERS_RDL    = {
                           0x01: (BinaryPayloadBuilder.add_16bit_uint, 2 ),
                           0x02: (BinaryPayloadBuilder.add_16bit_int,  2 ),
                           0x03: (BinaryPayloadBuilder.add_16bit_int, -178 ),
                           }


class modbus_server_t(object):
    def __init__(self, port, logger=None):
        if logger is None:
            FORMAT = ('%(asctime)-15s %(threadName)-15s'
                      ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')
            logging.basicConfig(format=FORMAT)
            self._logger = log = logging.getLogger()
            if "DEBUG" in os.environ:
                log.setLevel(logging.DEBUG)
            else:
                log.setLevel(logging.CRITICAL)
        else:
            self._logger = log = logger

        self._port = port

        e53_slave_block = self._create_block(MODBUS_REGISTERS_E53, byteorder=endian_big, wordorder=endian_big)
        rif_slave_block = self._create_block(MODBUS_REGISTERS_RIF, byteorder=endian_big, wordorder=endian_little)
        rdl_slave_block = self._create_block(MODBUS_REGISTERS_RDL, byteorder=endian_big, wordorder=endian_big)

        slaves = {MODBUS_DEV_ADDRESS_E53 : ModbusSlaveContext(hr=e53_slave_block, zero_mode=True),
                  MODBUS_DEV_ADDRESS_RIF : ModbusSlaveContext(ir=rif_slave_block, zero_mode=True),
                  MODBUS_DEV_ADDRESS_RDL : ModbusSlaveContext(ir=rdl_slave_block, zero_mode=True),
                  }
        self._context = ModbusServerContext(slaves=slaves, single=False)
        self._identity = ModbusDeviceIdentification()
        self._identity.VendorName = 'Pymodbus'
        self._identity.ProductCode = 'PM'
        self._identity.VendorUrl = 'http://github.com/riptideio/pymodbus/'
        self._identity.ProductName = 'Pymodbus Server'
        self._identity.ModelName = 'Pymodbus Server'
        self._identity.MajorMinorRevision = version_short

    def run_forever(self):
        log = self._logger
        StartSerialServer(context=self._context, framer=ModbusRtuFramer, identity=self._identity,
                      port=self._port, timeout=1, baudrate=9600)

    def _create_block(self, src, byteorder, wordorder):
        builder = BinaryPayloadBuilder(byteorder=byteorder, wordorder=wordorder)
        dst = {}
        for key, value in src.items():
            type_func, somevalue = value
            type_func(builder, somevalue)
            data = builder.to_registers()
            for n in range(len(data)):
                dst[key + n] = data[n]
            builder.reset()
        return ModbusSparseDataBlock(values=dst)


def main(args):
    if len(args) > 1:
        dev = args[1]
    else:
        osm_loc = os.getenv("OSM_LOC", "/tmp/osm/")
        dev = os.path.join(osm_loc, "UART_EXT_slave")
        if not os.path.exists(osm_loc):
            os.mkdir(osm_loc)
    modbus_server = modbus_server_t(dev)
    try:
        modbus_server.run_forever()
    except KeyboardInterrupt:
            print("modbus_server_t : Caught keyboard interrupt, exiting")
    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
