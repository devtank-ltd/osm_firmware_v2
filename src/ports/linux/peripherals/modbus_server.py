#!/usr/bin/env python3
import os
import logging
try:
    from pymodbus.version import version
except ModuleNotFoundError:
    import pymodbus
    version_short = pymodbus.__version__
    version_major = int(version_short.split(".")[0])
    version_minor = int(version_short.split(".")[1])
else:
    version_major = version.major
    version_minor = version.minor
    version_short = version.short
if version_major < 3:
    from pymodbus.server.sync import StartSerialServer
else:
    from pymodbus.server import StartSerialServer
if version_major >= 3 and version_minor > 1:
    FRAMER = "rtu"
else:
    from pymodbus.transaction import ModbusRtuFramer as FRAMER

try:
    from pymodbus.device import ModbusDeviceIdentification           # pymodbus < 3.10
except ImportError:
    from pymodbus import ModbusDeviceIdentification                  # pymodbus >= 3.10
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext, ModbusSparseDataBlock

BIG    = "big"
LITTLE = "little"

_convert_to_registers = None
if "MODBUS_LEGACY_PAYLOAD" not in os.environ:
    try:
        from pymodbus.client.mixin import ModbusClientMixin
        _convert_to_registers = ModbusClientMixin.convert_to_registers
        _DT = ModbusClientMixin.DATATYPE
        _DATATYPES = {"uint16":  _DT.UINT16,
                      "int16":   _DT.INT16,
                      "uint32":  _DT.UINT32,
                      "int32":   _DT.INT32,
                      "float32": _DT.FLOAT32,
                      "uint64":  _DT.UINT64,
                      "int64":   _DT.INT64,
                      "float64": _DT.FLOAT64,
                      }
    except (ImportError, AttributeError):
        _convert_to_registers = None

if _convert_to_registers is not None:
    PAYLOAD_BACKEND = "convert_to_registers"

    def _encode_big(value, data_type):
        return list(_convert_to_registers(value, _DATATYPES[data_type]))
else:
    PAYLOAD_BACKEND = "BinaryPayloadBuilder"
    from pymodbus.payload import BinaryPayloadBuilder
    from pymodbus.constants import Endian
    try:
        _endian_big = Endian.Big
    except AttributeError:
        _endian_big = Endian.BIG

    _BUILDER_FUNCS = {"uint16":  BinaryPayloadBuilder.add_16bit_uint,
                      "int16":   BinaryPayloadBuilder.add_16bit_int,
                      "uint32":  BinaryPayloadBuilder.add_32bit_uint,
                      "int32":   BinaryPayloadBuilder.add_32bit_int,
                      "float32": BinaryPayloadBuilder.add_32bit_float,
                      "uint64":  BinaryPayloadBuilder.add_64bit_uint,
                      "int64":   BinaryPayloadBuilder.add_64bit_int,
                      "float64": BinaryPayloadBuilder.add_64bit_float,
                      }

    def _encode_big(value, data_type):
        builder = BinaryPayloadBuilder(byteorder=_endian_big, wordorder=_endian_big)
        _BUILDER_FUNCS[data_type](builder, value)
        return list(builder.to_registers())


def encode_registers(value, data_type, byteorder=BIG, wordorder=BIG):
    regs = _encode_big(value, data_type)
    if wordorder == LITTLE:
        regs.reverse()
    if byteorder == LITTLE:
        regs = [((r & 0xFF) << 8) | (r >> 8) for r in regs]
    return regs


MODBUS_DEV_ADDRESS_E53  = 0x5
MODBUS_REGISTERS_E53    = {0xc56e: ("uint32",  1000),  # PF    "PowerFactorP1",  "PF"
                           0xc552: ("uint32", 24001),  # cVP1  "VoltageP1",      "V / 100"         , 24000 for 240 in centivolts
                           0xc554: ("uint32", 24002),  # cVP2  "VoltageP2",      "V / 100"         ,
                           0xc556: ("uint32", 24003),  # cVP3  "VoltageP3",      "V / 100"         ,
                           0xc560: ("uint32",  3001),  # mAP1  "CurrentP1",      "A / 1000"        , 3000 for 3A in milliamps
                           0xc562: ("uint32",  3002),  # mAP2  "CurrentP2",      "A / 1000"        ,
                           0xc564: ("uint32",  3003),  # mAP3  "CurrentP3",      "A / 1000"        ,
                           0xc652: ("uint32",  1000)   # ImEn  "ImportEnergy",   "watt/hours/0.001", 1000 for 1.0
                           }

MODBUS_DEV_ADDRESS_RIF  = 0x1
MODBUS_REGISTERS_RIF    = {0x36: ("float32", 1. ),   # PF   "PowerFactorP1",  "PF"
                           0x00: ("float32", 240.1), # VP1  "VoltageP1",      "V"
                           0x02: ("float32", 240.2), # VP2  "VoltageP2",      "V"
                           0x04: ("float32", 240.3), # VP3  "VoltageP3",      "V"
                           0x10: ("float32", 30.1),  # AP1  "CurrentP1",      "A"
                           0x12: ("float32", 30.2),  # AP2  "CurrentP2",      "A"
                           0x14: ("float32", 30.3),  # AP3  "CurrentP3",      "A"
                           0x60: ("float32", 1.)     # Imp  "ImportEnergy",   "watt/hours/1"
                           }
MODBUS_DEV_ADDRESS_RDL  = 0x2
MODBUS_REGISTERS_RDL    = {
                           0x01: ("uint16", 2 ),
                           0x02: ("int16",  2 ),
                           0x03: ("int16", -178 ),
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

        log.debug("pymodbus %s, payload backend: %s", version_short, PAYLOAD_BACKEND)

        self._port = port

        e53_slave_block = self._create_block(MODBUS_REGISTERS_E53, byteorder=BIG, wordorder=BIG)
        rif_slave_block = self._create_block(MODBUS_REGISTERS_RIF, byteorder=BIG, wordorder=LITTLE)
        rdl_slave_block = self._create_block(MODBUS_REGISTERS_RDL, byteorder=BIG, wordorder=BIG)


        slaves = {MODBUS_DEV_ADDRESS_E53 : ModbusSlaveContext(hr=e53_slave_block),
                  MODBUS_DEV_ADDRESS_RIF : ModbusSlaveContext(ir=rif_slave_block),
                  MODBUS_DEV_ADDRESS_RDL : ModbusSlaveContext(ir=rdl_slave_block),
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
        StartSerialServer(context=self._context, framer=FRAMER, identity=self._identity,
                      port=self._port, timeout=1, baudrate=9600)

    def _create_block(self, src, byteorder, wordorder, zero=True):
        dst = {}
        for key, (data_type, value) in src.items():
            data = encode_registers(value, data_type, byteorder=byteorder, wordorder=wordorder)
            base = (key + 1) if zero else key
            for n, reg in enumerate(data):
                dst[base + n] = reg
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
