#!/usr/bin/env python3

import logging
from pymodbus.transaction import ModbusBinaryFramer
from pymodbus.version import version
from pymodbus.server.sync import StartSerialServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext, ModbusSparseDataBlock


MODBUS_DEV_ADDRESS_E53  = 0x5
MODBUS_REGISTERS_E53    = {0xc56e: [0, 1],     # PF  "PowerFactorP1",  "PF"
                           0xc552: [0, 24001], # cVP1  "VoltageP1",      "V / 100"         , 24000 for 240 in centivolts
                           0xc554: [0, 24002], # cVP2  "VoltageP2",      "V / 100"         ,
                           0xc556: [0, 24003], # cVP3  "VoltageP3",      "V / 100"         ,
                           0xc560: [0, 3001],  # mAP1  "CurrentP1",      "A / 1000"        , 3000 for 3A in milliamps
                           0xc562: [0, 3002],  # mAP2  "CurrentP2",      "A / 1000"        ,
                           0xc564: [0, 3003],  # mAP3  "CurrentP3",      "A / 1000"        ,
                           0xc652: [0, 1000]   # ImEn  "ImportEnergy",   "watt/hours/0.001", 1000 for 1.0
                           }

MODBUS_DEV_ADDRESS_RIF  = 0x1
MODBUS_REGISTERS_RIF    = {0x36: [0, 1.],     # PF   "PowerFactorP1",  "PF"
                           0x00: [0, 24001.], # VP1  "VoltageP1",      "V"
                           0x02: [0, 24002.], # VP2  "VoltageP2",      "V"
                           0x04: [0, 24003.], # VP3  "VoltageP3",      "V"
                           0x10: [0, 3001.],  # AP1  "CurrentP1",      "A"
                           0x12: [0, 3002.],  # AP2  "CurrentP2",      "A"
                           0x14: [0, 3003.],  # AP3  "CurrentP3",      "A"
                           0x60: [0, 1000.]   # Imp  "ImportEnergy",   "watt/hours/1"
                           }

class meter_blocks_t(ModbusSparseDataBlock):

    def __init__(self, data):

        self._data = data
        super().__init__(values=self._data)

    def validate(self, address, count):
        data = self._data.get(address, None)
        if not data:
            return False
        if count > 2:
            return False
        return True

    def getValues(self, address, count):
        data = self._data.get(address, None)
        if data is None:
            return  None
        return data[0:count]


class modbus_server_t(object):
    def __init__(self, port, logger=None):
        if logger is None:
            FORMAT = ('%(asctime)-15s %(threadName)-15s'
                      ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')
            logging.basicConfig(format=FORMAT)
            self._logger = log = logging.getLogger()
            log.setLevel(logging.DEBUG)
        else:
            self._logger = log = logger

        self._port = port

        e53_slave_block = meter_blocks_t(MODBUS_REGISTERS_E53)
        rif_slave_block = meter_blocks_t(MODBUS_REGISTERS_RIF)
        slaves = {MODBUS_DEV_ADDRESS_E53 : ModbusSlaveContext(hr=e53_slave_block, zero_mode=True),
                  MODBUS_DEV_ADDRESS_RIF : ModbusSlaveContext(hr=rif_slave_block, zero_mode=True)}
        self._context = ModbusServerContext(slaves=slaves, single=False)
        self._identity = ModbusDeviceIdentification()
        self._identity.VendorName = 'Pymodbus'
        self._identity.ProductCode = 'PM'
        self._identity.VendorUrl = 'http://github.com/riptideio/pymodbus/'
        self._identity.ProductName = 'Pymodbus Server'
        self._identity.ModelName = 'Pymodbus Server'
        self._identity.MajorMinorRevision = version.short()

    def run_forever(self):
        log = self._logger
        StartSerialServer(self._context, framer=ModbusBinaryFramer, identity=self._identity,
                      port=self._port, timeout=1, baudrate=9600)


def main(args):
    modbus_server = modbus_server_t(args[1])
    modbus_server.run_forever()
    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
