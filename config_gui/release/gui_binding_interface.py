import binding
import serial
import time
import queue
import logging

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

logging.basicConfig(
    format='[%(asctime)s.%(msecs)06d] INTERFACE : %(message)s',
    level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S')

def log(msg):
    logging.info(msg)


class binding_interface_svr_t:
    def __init__(self):
        self._actions = { "REQ_MEAS" : self._request_meas_cb,
                          "REQ_MODBUS" : self._request_modbus_cb,
                          "REQ_OPEN" : self._open_con_cb,
                          "REQ_SERIAL_NUM" : lambda x : self.dev.serial_num.value,
                          "REQ_FIRMWARE" : lambda x : self.dev.version.value,
                          "REQ_DEV_EUI" : lambda x : self.dev.dev_eui,
                          "REQ_APP_KEY" : lambda x : self.dev.app_key,
                          "REQ_INT_MIN" : lambda x : self.dev.interval_mins,
                          "REQ_COMMS" : lambda x : self.dev.comms_conn.value,
                          "REQ_SAMP_COUNT" : self._request_sample,
                          "REQ_CHANGE_INT" : self._request_change_int,
                          "REQ_IOS" : self._request_ios,
                          "REQ_DO_CMD" : self._request_do_cmd,
                          "REQ_DBG" : self._request_dbg_mode,
                          "REQ_CHANGE_ALL_INT" : self._request_change_all_int,
                          "DEL_MB_REG" : self._request_del_reg,
                          "DEL_MB_DEV" : self._request_del_dev,
                          "REQ_CC_GAIN" : self._request_cc_gain,
                          "REQ_MIDP" : self._request_midpoint,
                          "UPDATE_MIDP" : self._update_midpoint,
                          "CC_CAL" : self._request_cc_cal,
                          "SET_CC" : self._request_set_cc,
                          "DO_CMD_MULTI" : self._request_do_cmd_multi,
                          "SAVE" : self._request_save,
                          "SET_EUI" : self._req_save_eui,
                          "SET_APP" : self._req_save_app,
                          "RAND_EUI" : self._req_rand_eui,
                          "RAND_APP" : self._req_rand_app,
                          "SET_DBG" : self._req_set_dbg,
                          "ADD_DEV" : self._req_add_dev,
                          "ADD_REG" : self._req_add_reg,
                          "REQ_PARSE" : self._req_parse_msg,
                          "REQ_DEBUG_BEGIN" : self._req_debug_begin,
                          "REQ_DEBUG_END" : self._req_debug_end}

        self.serial_obj = None
        self.dev = None
        self.debug_parse = None
    
    def _req_add_dev(self, args):
        unit_id = args[1]
        dev = args[2]
        is_msb = args[3]
        is_msw = args[4]
        self.dev.modbus_dev_add(unit_id, dev, is_msb, is_msw)
    
    def _req_add_reg(self, args):
        slave_id = args[1]
        reg = args[2]
        hex_add = args[3]
        func_type = args[4]
        data_type = args[5]
        dev = binding.modbus_reg_t(self.dev, reg, hex_add, func_type, data_type)
        self.dev.modbus_reg_add(slave_id,
            dev)
        
    def _req_set_dbg(self, args):
        val = args[1]
        self.dev._set_debug(val)
    
    def _req_rand_eui(self, args):
        return self.dev.write_eui()
    
    def _req_rand_app(self, args):
        return self.dev.write_lora()
    
    def _req_save_eui(self, args):
        eui = args[1]
        self.dev.dev_eui = eui
    
    def _req_save_app(self, args):
        app = args[1]
        self.dev.app_key = app
    
    def _request_save(self, args):
        self.dev.save()

    def _req_parse_msg(self, args):
        msg = args[1]
        returned = self.debug_parse.parse_msg(msg)
        return returned
    
    def _request_do_cmd_multi(self, args):
        cmd = args[1]
        log(f"in request_do_cmd_multi: {cmd}")
        return self.dev.do_cmd_multi(cmd)
    
    def _request_set_cc(self, args):
        phase = args[1]
        outer = args[2]
        inner = args[3]
        self.dev.set_outer_inner_cc(phase, outer, inner)
    
    def _request_cc_cal(self, args):
        self.dev.current_clamp_calibrate()
    
    def _update_midpoint(self, args):
        mp = args[1]
        widget = args[2]
        self.dev.update_midpoint(mp, widget)

    def _request_midpoint(self, args):
        cc = args[1]
        return self.dev.get_midpoint(cc)

    def _request_del_dev(self, args):
        dev = args[1]
        self.dev.modbus_dev_del(dev)

    def _request_del_reg(self, args):
        reg = args[1]
        self.dev.modbus_reg_del(reg)
    
    def _request_change_all_int(self, args):
        val = args[1]
        self.dev.interval_mins = val
    
    def _request_dbg_parse(self, args):
        line = args[1]
        self.debug_parse.parse_msg(line)

    def _request_dbg_mode(self, args):
        return self.dev.dbg_readlines()
    
    def _request_do_cmd(self, args):
        cmd = args[1]
        self.dev.do_cmd(cmd)
    
    def _request_ios(self, args):
        io = args[1]
        io = self.dev.ios[io]
        return self.dev.ios[io]
    
    def _request_change_int(self, args):
        meas = args[1]
        widget = args[2]
        self.dev.change_interval(meas, widget)
    
    def _request_sample(self, args):
        meas = args[1]
        widget = args[2]
        self.dev.change_samplec(meas, widget)

    def _request_meas_cb(self, args):
        if not self.dev:
            return []
        return self.dev.measurements()

    def _request_modbus_cb(self, args):
        if not self.dev:
            return []
        m = self.dev.get_modbus()
        devs = m.devices
        mmnts = []
        if devs:
            for dev in devs:
                for reg in dev.regs:
                    i = (str(reg).split(','))
                    i.insert(0, str(dev.name))
                    mmnts.append(i)
        return mmnts

    def _open_con_cb(self, args):
        dev = args[1]
        log(f"Openning serial:" + dev)
        try:
            serial_obj = serial.Serial(port=dev,
                                            baudrate=115200,
                                            bytesize=serial.EIGHTBITS,
                                            parity=serial.PARITY_NONE,
                                            stopbits=serial.STOPBITS_ONE,
                                            timeout=0)
            self.dev = binding.dev_t(serial_obj)
            self.serial_obj = serial_obj
            self.debug_parse = None
            return True
        except Exception as e:
            log(f"Openned Failed {e}")
            # Todo, handle expected or error out
            return False

    def _req_debug_begin(self, args):
        if not self.debug_parse:
            self.dev._set_debug(1)
            self.debug_parse = binding.dev_debug_t(self.serial_obj)

    def _req_debug_end(self, args):
        if self.debug_parse:
            self.dev._set_debug(0)
        self.debug_parse = None


    def run_forever(self, in_queue, out_queue, timeout=1):
        while True:
            if self.debug_parse:
                debug_lines = self.debug_parse.read_msgs(0.1)
                for debug_line in debug_lines:
                    out_queue.put(('*','DEBUG',debug_line))
                if not in_queue.empty():
                    try:
                        data_in = in_queue.get(False, timeout)
                    except queue.Empty:
                        data_in = None
            else:
                try:
                    data_in = in_queue.get(False, timeout)
                except queue.Empty:
                    data_in = None
            if data_in:
                log(f"Got {data_in}")
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
        self.unsolicited_handlers = {}

    def process_message(self):
        if self.out_queue.empty():
            if self._answered_cbs.empty():
                return None
            else:
                return False
        output = self.out_queue.get()
        if output[0] == '*':
            # unsolicited message
            unsolicited_handler = self.unsolicited_handlers.get(output[1])
            if unsolicited_handler:
                unsolicited_handler(output)
            return True
        cb = self._answered_cbs.get()
        try:
            cb(output)
        except Exception as e:
            log(f"Exception in process message: {e}")
        return True

    def _basic_query(self, cmd, answered_cb):
        log(f"Sending {cmd}")
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

    def get_comms_conn(self, answered_cb):
        self._basic_query(("REQ_COMMS",), answered_cb)
    
    def get_measurements(self, answered_cb):
        self._basic_query(("REQ_MEAS",), answered_cb)

    def change_sample(self, meas, widget):
        self._basic_query(("REQ_SAMP_COUNT", meas, widget), None)
    
    def change_interval(self, meas, widget):
        self._basic_query(("REQ_CHANGE_INT", meas, widget), None)

    def get_ios(self, num):
        self._basic_query(("REQ_IOS", num), None)
    
    def send_cmd(self, cmd):
        self._basic_query(("REQ_DO_CMD", cmd), None)
    
    def dbg_readlines(self, answered_cb):
        self._basic_query(("REQ_DBG",), answered_cb)
    
    def debug_parse(self, line, answered_cb):
        self._basic_query(("REQ_PARSE", line), answered_cb)
    
    def set_interval_mins(self, val):
        self._basic_query(("REQ_CHANGE_ALL_INT", val), None)
    
    def get_modbus(self, answered_cb):
        self._basic_query(("REQ_MODBUS",), answered_cb)
    
    def modbus_reg_del(self, reg):
        self._basic_query(("DEL_MB_REG", reg), None)

    def modbus_dev_del(self, dev):
        self._basic_query(("DEL_MB_DEV", dev), None)
    
    def print_cc_gain(self):
        self._basic_query(("REQ_CC_GAIN",), None)

    def get_midpoint(self, widget):
        self._basic_query(("REQ_MIDP", widget), None)
    
    def update_midpoint(self, mp, widget):
        self._basic_query(("UPDATE_MIDP", mp, widget), None)

    def current_clamp_calibrate(self):
        self._basic_query(("CC_CAL",), None)
    
    def set_outer_inner_cc(self, phase, outer, inner):
        self._basic_query(("SET_CC", phase, outer, inner), None)
    
    def do_cmd_multi(self, cmd, answered_cb):
        log(f"in do_cmd_multi interface {cmd}")
        self._basic_query(("DO_CMD_MULTI", cmd), answered_cb)
    
    def save_config(self):
        self._basic_query(("SAVE",), None)
    
    def set_app_key(self, app):
        self._basic_query(("SET_APP", app), None)
    
    def set_eui_key(self, eui):
        self._basic_query(("SET_EUI", eui), None)
    
    def gen_rand_eui(self):
        self._basic_query(("RAND_EUI",), None)

    def gen_rand_app(self):
        self._basic_query(("RAND_APP",), None)
    
    def set_debug(self, val):
        self._basic_query(("SET_DBG", val), None)
    
    def add_modbus_dev(self, slave_id, device, is_msb, is_msw):
        self._basic_query(("ADD_DEV", slave_id, device, is_msb, is_msw), None)
    
    def add_modbus_reg(self, slave_id, reg, hex_add, func, data_type):
        self._basic_query(("ADD_REG", slave_id, reg, hex_add, func, data_type), None)

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
        log(f"Got FW: {firmware_ver}")

    def _on_get_serial_cb(resp):
        assert not resp[0].startswith("ERROR"), "Failed to get serial"
        serial_num = resp[1]
        assert isinstance(serial_num, str), "Not expected response"
        log(f"Got Serial: {serial_num}")

    def _on_open_done_cb(resp):
        assert not resp[0].startswith("ERROR"), "Failed to get serial"
        success = resp[1]
        assert success, "Failed to open serial"
        log(f"Opened")

    binding_interface.open("/tmp/osm/UART_DEBUG_slave", _on_open_done_cb)
    binding_interface.get_serial_num(_on_get_serial_cb)
    binding_interface.get_fw_version(_on_get_fw_cb)
    binding_interface.change_sample("FW", 2)

    while binding_interface.process_message() != None:
        time.sleep(0.1)
    log(f"Waiting for cleanup")
    binding_process.terminate()
    binding_process.join()
