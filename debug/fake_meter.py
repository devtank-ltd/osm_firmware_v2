import sys

# --------------------------------------------------------------------------- #
# import the various server implementations
# --------------------------------------------------------------------------- #
from pymodbus.version import version
#from pymodbus.server.sync import StartTcpServer
#from pymodbus.server.sync import StartTlsServer
#from pymodbus.server.sync import StartUdpServer
from pymodbus.server.sync import StartSerialServer

from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSparseDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext

from pymodbus.transaction import ModbusRtuFramer, ModbusBinaryFramer
# --------------------------------------------------------------------------- #
# configure the service logging
# --------------------------------------------------------------------------- #
import logging
FORMAT = ('%(asctime)-15s %(threadName)-15s'
          ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')
logging.basicConfig(format=FORMAT)
log = logging.getLogger()
log.setLevel(logging.DEBUG)


class FakeMeterBlocks(ModbusSparseDataBlock):
    """ I'm not sure why this is required, I think ModbusSparseDataBlock doesn't support such large addresses? """

    def __init__(self):

        self._data = {0xc56e: [0, 0],     # PF  "PowerFactorP1",  "PF"              
                      0xc552: [0, 24000], # cVP1  "VoltageP1",      "V / 100"         , 24000 for 240 in centivolts
                      0xc554: [0, 24000], # cVP2  "VoltageP2",      "V / 100"         ,                        
                      0xc556: [0, 24000], # cVP3  "VoltageP3",      "V / 100"         ,                        
                      0xc560: [0, 3000],  # mAP1  "CurrentP1",      "A / 1000"        , 3000 for 3A in milliamps
                      0xc562: [0, 3000],  # mAP2  "CurrentP2",      "A / 1000"        ,                        
                      0xc564: [0, 3000],  # mAP3  "CurrentP3",      "A / 1000"        ,                        
                      0xc652: [0, 1000]   # ImEn  "ImportEnergy",   "watt/hours/0.001", 1000 for 1.0
                      }
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


block = FakeMeterBlocks()

slaves = { 0x5 : ModbusSlaveContext(hr=block, zero_mode=True) }

context = ModbusServerContext(slaves=slaves, single=False)

# ----------------------------------------------------------------------- #
# initialize the server information
# ----------------------------------------------------------------------- #
# If you don't set this or any fields, they are defaulted to empty strings.
# ----------------------------------------------------------------------- #
identity = ModbusDeviceIdentification()
identity.VendorName = 'Pymodbus'
identity.ProductCode = 'PM'
identity.VendorUrl = 'http://github.com/riptideio/pymodbus/'
identity.ProductName = 'Pymodbus Server'
identity.ModelName = 'Pymodbus Server'
identity.MajorMinorRevision = version.short()


StartSerialServer(context, framer=ModbusBinaryFramer, identity=identity,
                  port=sys.argv[1], timeout=1, baudrate=9600)
