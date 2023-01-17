import json
import select
import binding
import string
import serial
import sys
import time
import queue

# Interface between API and binding

# * Open Server
# REQ_OPEN <PATH>

# * Request Measurements
# REQ_MEAS

# * Answer Measurements
# RSP_MEAS {json}

# * Request Modbus
# REQ_MODBUS

# * Answer Modbus
# RSP_MODBUS {son}

class binding_interface_svr_t:
    def __init__(self):
        self._actions = { "REQ_MEAS" : self._request_meas_cb,
                          "REQ_MODBUS" : self._request_modbus_cb,
                          "REQ_OPEN" : self._open_con_cb,
                          "REQ_SERIAL_NUM" : lambda x : self.dev.serial_num.value,
                          "REQ_FIRMWARE" : lambda x : self.dev.version.value,
                          "REQ_DEV_EUI" : lambda x : self.dev.dev_eui,
                          "REQ_APP_KEY" : lambda x : self.dev.app_key,
                          "REQ_INT_MIN" : lambda x : self.dev.interval_mins}
        self.dev = None
        self.debug_parse = None

    def _request_meas_cb(self, args):
        if not self.dev:
            return []
        return self.dev.measurements()

    def _request_modbus_cb(self, args):
        if not self.dev:
            return []
        m = self.dev.get_modbus()
        devs = m.devices
        if devs:
            mmnts = []
            for dev in devs:
                for reg in dev.regs:
                    i = (str(reg).split(','))
                    i.insert(0, str(dev.name))
                    mmnts.append(i)
        return mmnts

    def _open_con_cb(self, args):
        dev = args[1]
        print("Openning serial:" + dev)
        try:
            serial_obj = serial.Serial(port=dev,
                                            baudrate=115200,
                                            bytesize=serial.EIGHTBITS,
                                            parity=serial.PARITY_NONE,
                                            stopbits=serial.STOPBITS_ONE,
                                            timeout=0)
            self.dev = binding.dev_t(serial_obj)
            self.debug_parse = binding.dev_debug_t(serial_obj)
            return self.dev
        except Exception as e:
            print("Openned Failed", e)
            # Todo, handle expected or error out
            return False


    def run_forever(self, in_queue, out_queue, timeout=1):
        while True:
            try:
                data_in = in_queue.get(False, timeout)
            except queue.Empty:
                data_in = None
            if data_in:
                print("Got", data_in)
                action = self._actions.get(data_in[0], None)
                if action:
                    ret = action(data_in)
                else:
                    ret = "ERROR INAVLID"
                out_queue.put((data_in[0].replace("REQ","RSP"), ret))


class binding_interface_client_t:
    def __init__(self, in_queue, out_queue):
        self.in_queue = in_queue
        self.out_queue = out_queue
        self._answered_cbs = queue.SimpleQueue()

    def process_message(self):
        if self.out_queue.empty():
            if self._answered_cbs.empty():
                return None
            else:
                return False
        cb = self._answered_cbs.get()
        cb(self.out_queue.get())
        return True

    def _basic_query(self, cmd, answered_cb):
        print("Sending", cmd)
        self.in_queue.put(cmd)
        self._answered_cbs.put(answered_cb)
        
    def open(self, path, answered_cb):
        self._basic_query(("REQ_OPEN", path), answered_cb)

    def get_serial_num(self, answered_cb):
        self._basic_query(("REQ_SERIAL_NUM",), answered_cb)

    def get_fw_version(self, answered_cb):
        self._basic_query(("REQ_FIRMWARE",), answered_cb)

    def get_dev_eui(self, answered_cb):
        self._basic_query(("REQ_DEV_EUI",), answered_cb)

    def get_app_key(self, answered_cb):
        self._basic_query(("REQ_APP_KEY",), answered_cb)
    
    def get_interval_min(self, answered_cb):
        self._basic_query(("REQ_INT_MIN",), answered_cb)


if __name__ == "__main__":
    from multiprocessing import Process, Queue

    def _binding_process_cb(binding_in, binding_out):
        binding_svr = binding_interface_svr_t()
        binding_svr.run_forever(binding_in, binding_out)

    binding_out = Queue()
    binding_in = Queue()

    binding_process = Process(target=_binding_process_cb, args=(binding_in, binding_out))
    binding_process.start()

    binding_interface = binding_interface_client_t(binding_in, binding_out)

    def _on_get_fw_cb(resp):
        assert not resp[0].startswith("ERROR"), "Failed to get firmware"
        firmware_ver = resp[1]
        assert isinstance(firmware_ver, str), "Not expected response"
        print("Got FW: " + firmware_ver)

    def _on_get_serial_cb(resp):
        assert not resp[0].startswith("ERROR"), "Failed to get serial"
        serial_num = resp[1]
        assert isinstance(serial_num, str), "Not expected response"
        print("Got Serial: " + serial_num)

    def _on_open_done_cb(resp):
        assert not resp[0].startswith("ERROR"), "Failed to get serial"
        success = resp[1]
        assert success, "Failed to open serial"
        print("Openned")

    binding_interface.open("/tmp/osm/UART_DEBUG_slave", _on_open_done_cb)
    binding_interface.get_serial_num(_on_get_serial_cb)
    binding_interface.get_fw_version(_on_get_fw_cb)

    while binding_interface.process_message() != None:
        time.sleep(0.1)
    print("Waiting for cleanup")
    binding_process.terminate()
    binding_process.join()
