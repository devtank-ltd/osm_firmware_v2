import json
import select
import binding
import string
import serial
import sys
import time

# Interface between API and binding

# * Request Measurements
# REQ_MEAS

# * Answer Measurements
# RSP_MEAS {json}

# * Request Modbus
# REQ_MODBUS

# * Answer Modbus
# RSP_MODBUS {son}

class interface_t:
    def __init__(self, dev):
        self._actions = { "REQ_MEAS" : self._request_meas_cb,
                          "REQ_MODBUS" : self._request_modbus_cb}
        self._dev = dev

    def _request_meas_cb(self, args):
        return json.dumps(self._dev.measurements())

    def _request_modbus_cb(self, args):
        m = self._dev.get_modbus()
        return json.dumps(self._dev.get_modbus())

    def run_forever(self, timeout=1):
        action = None
        now = time.monotonic()
        end_time = now + timeout
        while now < end_time:
            r = select.select([sys.stdin],[],[], timeout)
            if len(r[0]):
                line = sys.stdin.readline()
                sys.stderr.write("LINE:" + line)
                parts = line.split()
                if parts:
                    action = self._actions.get(parts[0], None)
                if action:
                    ret = action(parts)
                else:
                    ret = "ERROR INAVLID"
                if parts:
                    ret = parts[0].replace("REQ","RSP") +":" + ret
                    sys.stdout.write(ret)
"""
    # ==================================================================================

    def process_worker_resp_line(self, line):
        if line.startswith("ERROR"):
            print("OH BUGGER!")
        elif line.startswith("RSP_MODBUS"):
            self._process_rsp_modbus(json.loads(line.replace("RSP_MODBUS:","")))
        elif line.startswith("REQ_MEAS"):
            self._process_rsp_meas(json.loads(line.replace("RSP_MODBUS:","")))


    def _step(self, count=0):
        r = select.select([self.worker_stdout],[],[],timeout=0)
        if len(r[0]):
            line = self.worker_stdout.readline()
            self.process_worker_resp_line(line)
"""

if __name__ == '__main__':
    serial_obj = serial.Serial(port="/tmp/osm/UART_DEBUG_slave",
                                    baudrate=115200,
                                    bytesize=serial.EIGHTBITS,
                                    parity=serial.PARITY_NONE,
                                    stopbits=serial.STOPBITS_ONE,
                                    timeout=0)
    device = binding.dev_t(serial_obj)
    interface_t(device).run_forever()
