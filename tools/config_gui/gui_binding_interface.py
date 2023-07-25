from multiprocessing import Process, Queue
import traceback
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

REQ_EXIT            = "REQ_EXIT"
REQ_MEAS            = "REQ_MEAS"
REQ_MODBUS          = "REQ_MODBUS"
REQ_OPEN            = "REQ_OPEN"
REQ_SERIAL_NUM      = "REQ_SERIAL_NUM"
REQ_FIRMWARE        = "REQ_FIRMWARE"
REQ_DEV_EUI         = "REQ_DEV_EUI"
REQ_APP_KEY         = "REQ_APP_KEY"
REQ_INT_MIN         = "REQ_INT_MIN"
REQ_COMMS           = "REQ_COMMS"
REQ_SAMP_COUNT      = "REQ_SAMP_COUNT"
REQ_CHANGE_INT      = "REQ_CHANGE_INT"
REQ_IOS             = "REQ_IOS"
REQ_DO_CMD          = "REQ_DO_CMD"
REQ_CHANGE_ALL_INT  = "REQ_CHANGE_ALL_INT"
REQ_DEL_MB_REG      = "REQ_DEL_MB_REG"
REQ_DEL_MB_DEV      = "REQ_DEL_MB_DEV"
REQ_CC_GAIN         = "REQ_CC_GAIN"
REQ_MIDP            = "REQ_MIDP"
REQ_UPDATE_MIDP     = "REQ_UPDATE_MIDP"
REQ_CC_CAL          = "REQ_CC_CAL"
REQ_SET_CC          = "REQ_SET_CC"
REQ_DO_CMD_MULTI    = "REQ_DO_CMD_MULTI"
REQ_SAVE            = "REQ_SAVE"
REQ_SET_EUI         = "REQ_SET_EUI"
REQ_SET_APP         = "REQ_SET_APP"
REQ_RAND_EUI        = "REQ_RAND_EUI"
REQ_RAND_APP        = "REQ_RAND_APP"
REQ_ADD_DEV         = "REQ_ADD_DEV"
REQ_ADD_REG         = "REQ_ADD_REG"
REQ_DEBUG_BEGIN     = "REQ_DEBUG_BEGIN"
REQ_DEBUG_END       = "REQ_DEBUG_END"
REQ_DIS_IO          = "REQ_DIS_IO"
REQ_ACT_IO          = "REQ_ACT_IO"
REQ_SET_COEFFS      = "REQ_SET_COEFFS"
REQ_FTMA            = "REQ_FTMA"
REQ_SET_FTMA_NAME   = "REQ_SET_FTMA_NAME"
REQ_GET_FTMA_COEFFS = "REQ_GET_FTMA_COEFFS"
REQ_SAVE_CONF       = "REQ_SAVE_CONF"
REQ_LOAD_CONF       = "REQ_LOAD_CONF"
REQ_LAST_VAL        = "REQ_LAST_VAL"

logging.basicConfig(
    format='[%(asctime)s.%(msecs)06d] INTERFACE : %(message)s',
    level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S')

def log(msg):
    logging.info(msg)


class binding_interface_svr_t:
    def __init__(self):
        self._actions = { REQ_EXIT            : self._exit_cb,
                          REQ_MEAS            : self._request_meas_cb,
                          REQ_MODBUS          : self._request_modbus_cb,
                          REQ_OPEN            : self._open_con_cb,
                          REQ_SERIAL_NUM      : lambda x : self.dev.serial_num.value,
                          REQ_FIRMWARE        : lambda x : self.dev.version.value,
                          REQ_DEV_EUI         : lambda x : self.dev.dev_eui,
                          REQ_APP_KEY         : lambda x : self.dev.app_key,
                          REQ_INT_MIN         : lambda x : self.dev.interval_mins,
                          REQ_COMMS           : lambda x : self.dev.comms_conn.value,
                          REQ_SAMP_COUNT      : self._request_sample,
                          REQ_CHANGE_INT      : self._request_change_int,
                          REQ_IOS             : self._request_ios,
                          REQ_DO_CMD          : self._request_do_cmd,
                          REQ_CHANGE_ALL_INT  : self._request_change_all_int,
                          REQ_DEL_MB_REG      : self._request_del_reg,
                          REQ_DEL_MB_DEV      : self._request_del_dev,
                          REQ_CC_GAIN         : lambda x : self.dev.print_cc_gain,
                          REQ_MIDP            : self._request_midpoint,
                          REQ_UPDATE_MIDP     : self._update_midpoint,
                          REQ_CC_CAL          : self._request_cc_cal,
                          REQ_SET_CC          : self._request_set_cc,
                          REQ_DO_CMD_MULTI    : self._request_do_cmd_multi,
                          REQ_SAVE            : self._request_save,
                          REQ_SET_EUI         : self._req_save_eui,
                          REQ_SET_APP         : self._req_save_app,
                          REQ_RAND_EUI        : self._req_rand_eui,
                          REQ_RAND_APP        : self._req_rand_app,
                          REQ_ADD_DEV         : self._req_add_dev,
                          REQ_ADD_REG         : self._req_add_reg,
                          REQ_DEBUG_BEGIN     : self._req_debug_begin,
                          REQ_DEBUG_END       : self._req_debug_end,
                          REQ_DIS_IO          : self._req_disable_io,
                          REQ_ACT_IO          : self._req_activate_io,
                          REQ_SET_COEFFS      : self._req_set_coeffs,
                          REQ_FTMA            : self._req_ftma_specs,
                          REQ_SET_FTMA_NAME   : self._req_ftma_name,
                          REQ_GET_FTMA_COEFFS : self._req_ftma_coeffs,
                          REQ_SAVE_CONF       : self._save_config_to_json,
                          REQ_LOAD_CONF       : self._load_json_conf_to_osm,
                          REQ_LAST_VAL        : self._get_last_val}

        self.serial_obj = None
        self.dev = None
        self.debug_parse = None
        self._alive = True

    def _get_last_val(self, args):
        meas = args[1]
        val = self.dev.do_cmd(f"get_meas {meas}")
        return val

    def _load_json_conf_to_osm(self, args):
        dev = self.dev.create_json_dev(self.dev)
        filename = args[1]
        dev.verify_file(filename)

    def _save_config_to_json(self, args):
        dev = self.dev.create_json_dev(self.dev)
        dev.get_config()
        filename = args[1]
        return dev.save_config(filename)

    def _req_ftma_specs(self, args):
        headers = args
        return self.dev.get_ftma_types(headers)

    def _req_ftma_name(self, args):
        name = args[1]
        meas = args[2]
        return self.dev.set_ftma_name(name, meas)

    def _req_set_coeffs(self, args):
        a = args[1]
        b = args[2]
        c = args[3]
        d = args[4]
        meas = args[5]
        return self.dev.set_ftma_coeffs(a, b, c, d, meas)

    def _req_ftma_coeffs(self, args):
        meas = args[1]
        return self.dev.read_ftma_coeffs(meas)

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

    def _req_rand_eui(self, args):
        return self.dev.write_eui()

    def _req_rand_app(self, args):
        return self.dev.write_lora()

    def _req_save_eui(self, args):
        eui = args[1]
        self.dev.dev_eui = eui
        return self.dev.dev_eui

    def _req_save_app(self, args):
        app = args[1]
        self.dev.app_key = app
        return self.dev.app_key

    def _request_save(self, args):
        self.dev.save()

    def _request_do_cmd_multi(self, args):
        cmd = args[1]
        return self.dev.do_cmd_multi(cmd)

    def _request_set_cc(self, args):
        cc = args[1]
        outer = args[2]
        inner = args[3]
        self.dev.set_outer_inner_cc(cc, outer, inner)

    def _request_cc_cal(self, args):
        self.dev.current_clamp_calibrate()

    def _update_midpoint(self, args):
        mp = args[1]
        cc = args[2]
        self.dev.update_midpoint(mp, cc)

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

    def _request_do_cmd(self, args):
        cmd = args[1]
        self.dev.do_cmd(cmd)

    def _request_ios(self, args):
        io = args[1]
        self.io = self.dev.ios[io]
        active = self.io.active_as()
        pull = self.io.active_pull()
        return active, pull

    def _req_disable_io(self, args):
        self.io.disable_io()

    def _req_activate_io(self, args):
        self.io.activate_io(args[1], args[2], args[3])

    def _request_change_int(self, args):
        meas = args[1]
        value = args[2]
        self.dev.change_interval(meas, value)

    def _request_sample(self, args):
        meas = args[1]
        value = args[2]
        self.dev.change_samplec(meas, value)

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
        log(f"Opening serial:" + dev)
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
            traceback.print_exc()
            log(f"Opened Failed {e}")
            # Todo, handle expected or error out
            return False

    def _req_debug_begin(self, args):
        if not self.debug_parse:
            self.dev.do_cmd("debug_mode")
            self.debug_parse = binding.dev_debug_t(self.serial_obj)

    def _req_debug_end(self, args):
        if self.debug_parse:
            self.dev._set_debug(0)
        self.debug_parse = None

    def _exit_cb(self, args):
        self._alive = False

    def run_forever(self, in_queue, out_queue, timeout=1):
        while self._alive:
            data_in = None
            if self.debug_parse:
                debug_lines = self.debug_parse.read_msgs(0.1)
                if debug_lines:
                    for debug_line in debug_lines:
                        out_queue.put(('*','DEBUG',debug_line))
                    if not in_queue.empty():
                        data_in = in_queue.get()
            else:
                try:
                    data_in = in_queue.get(True, timeout)
                except queue.Empty:
                    pass
            if data_in:
                action = self._actions.get(data_in[0], None)
                if action:
                    ret = action(data_in)
                else:
                    ret = "ERROR INAVLID"
                answer = (data_in[0].replace("REQ","RSP"), ret)
                out_queue.put(answer)


def _binding_process_cb(binding_in, binding_out):
    binding_svr = binding_interface_svr_t()
    binding_svr.run_forever(binding_in, binding_out)
    log("Async Subprocess Finished")


class binding_interface_client_t:
    def __init__(self):
        self.in_queue =  Queue()
        self.out_queue =  Queue()
        self._answered_cbs = queue.SimpleQueue()
        self.unsolicited_handlers = {}
        self._binding_process = Process(target=_binding_process_cb, args=(self.in_queue, self.out_queue))
        self._binding_process.start()

    def finish(self):
        self._basic_query((REQ_EXIT,))
        while self.process_message() is None:
            time.sleep(0.01)
        log("Collecting aysnc subprocess")
        self._binding_process.join()

    def process_message(self):
        if self.out_queue.empty():
            if self._answered_cbs.empty():
                return None # Nothing to wait for
            else:
                return False # Input send but no response
        output = self.out_queue.get()
        if output[0] == '*':
            # unsolicited message
            unsolicited_handler = self.unsolicited_handlers.get(output[1])
            if unsolicited_handler:
                unsolicited_handler(output)
            return True
        log(f"Answered {output}")
        cb = self._answered_cbs.get()
        if cb:
            try:
                cb(output)
            except Exception as e:
                log(f"Exception in process message: {e}")
                traceback.print_exc()
        return True

    def _basic_query(self, cmd, answered_cb=None):
        log(f"Sending {cmd}")
        self.in_queue.put(cmd)
        self._answered_cbs.put(answered_cb)

    def open(self, path, answered_cb):
        self._basic_query((REQ_OPEN, path), answered_cb)

    def get_serial_num(self, answered_cb):
        self._basic_query((REQ_SERIAL_NUM,), answered_cb)

    def get_fw_version(self, answered_cb):
        self._basic_query((REQ_FIRMWARE,), answered_cb)

    def get_dev_eui(self, answered_cb):
        self._basic_query((REQ_DEV_EUI,), answered_cb)

    def get_app_key(self, answered_cb):
        self._basic_query((REQ_APP_KEY,), answered_cb)

    def get_interval_min(self, answered_cb):
        self._basic_query((REQ_INT_MIN,), answered_cb)

    def get_comms_conn(self, answered_cb):
        self._basic_query((REQ_COMMS,), answered_cb)

    def get_measurements(self, answered_cb):
        self._basic_query((REQ_MEAS,), answered_cb)

    def change_sample(self, meas, value, answered_cb=None):
        self._basic_query((REQ_SAMP_COUNT, meas, value), answered_cb)

    def change_interval(self, meas, value, answered_cb=None):
        self._basic_query((REQ_CHANGE_INT, meas, value), answered_cb)

    def get_ios(self, num, answered_cb):
        self._basic_query((REQ_IOS, num), answered_cb)

    def send_cmd(self, cmd, answered_cb=None):
        self._basic_query((REQ_DO_CMD, cmd), answered_cb)

    def set_interval_mins(self, val, answered_cb=None):
        self._basic_query((REQ_CHANGE_ALL_INT, val), answered_cb)

    def set_coeffs(self, a, b, c, d, meas, answered_cb=None):
        self._basic_query((REQ_SET_COEFFS, a, b, c, d, meas), answered_cb)

    def set_ftma_name(self, meas, name, answered_cb):
        self._basic_query((REQ_SET_FTMA_NAME, meas, name), answered_cb)

    def get_ftma_specs(self, headers, answered_cb):
        self._basic_query((REQ_FTMA, headers), answered_cb)

    def get_spec_ftma_coeffs(self, meas, answered_cb):
        self._basic_query((REQ_GET_FTMA_COEFFS, meas), answered_cb)

    def get_modbus(self, answered_cb):
        self._basic_query((REQ_MODBUS,), answered_cb)

    def modbus_reg_del(self, reg, answered_cb=None):
        self._basic_query((REQ_DEL_MB_REG, reg), answered_cb)

    def modbus_dev_del(self, dev, answered_cb=None):
        self._basic_query((REQ_DEL_MB_DEV, dev), answered_cb)

    def print_cc_gain(self, answered_cb):
        self._basic_query((REQ_CC_GAIN,), answered_cb)

    def get_midpoint(self, cc, answered_cb):
        self._basic_query((REQ_MIDP, cc), answered_cb)

    def update_midpoint(self, mp, cc, answered_cb):
        self._basic_query((REQ_UPDATE_MIDP, mp, cc), answered_cb)

    def current_clamp_calibrate(self, answered_cb):
        self._basic_query((REQ_CC_CAL,), answered_cb)

    def set_outer_inner_cc(self, cc, outer, inner, answered_cb):
        self._basic_query((REQ_SET_CC, cc, outer, inner), answered_cb)

    def do_cmd_multi(self, cmd, answered_cb):
        self._basic_query((REQ_DO_CMD_MULTI, cmd), answered_cb)

    def save_config(self, answered_cb=None):
        self._basic_query((REQ_SAVE,), None)

    def set_app_key(self, app, answered_cb=None):
        self._basic_query((REQ_SET_APP, app), answered_cb)

    def set_dev_eui(self, eui, answered_cb=None):
        self._basic_query((REQ_SET_EUI, eui), answered_cb)

    def gen_rand_eui(self, answered_cb=None):
        self._basic_query((REQ_RAND_EUI,), answered_cb)

    def gen_rand_app(self, answered_cb=None):
        self._basic_query((REQ_RAND_APP,), answered_cb)

    def add_modbus_dev(self, slave_id, device, is_msb, is_msw, answered_cb=None):
        self._basic_query((REQ_ADD_DEV, slave_id, device, is_msb, is_msw), answered_cb)

    def add_modbus_reg(self, slave_id, reg, hex_add, func, data_type, answered_cb=None):
        self._basic_query((REQ_ADD_REG, slave_id, reg, hex_add, func, data_type), answered_cb)

    def debug_begin(self, answered_cb=None):
        self._basic_query((REQ_DEBUG_BEGIN,), answered_cb)

    def debug_end(self, answered_cb=None):
        self._basic_query((REQ_DEBUG_END,), answered_cb)

    def disable_io(self, answered_cb=None):
        self._basic_query((REQ_DIS_IO,), answered_cb)

    def activate_io(self, inst, rise, pullup, answered_cb=None):
        self._basic_query((REQ_ACT_IO, inst, rise, pullup), answered_cb)

    def save(self, answered_cb=None):
        self._basic_query((REQ_SAVE,), answered_cb)

    def save_config_to_json(self, contents, answered_cb=None):
        self._basic_query((REQ_SAVE_CONF, contents), answered_cb)

    def write_json_to_osm(self, contents, answered_cb=None):
        self._basic_query((REQ_LOAD_CONF, contents), answered_cb)

    def get_last_value(self, meas, answered_cb=None):
        self._basic_query((REQ_LAST_VAL, meas,), answered_cb)


if __name__ == "__main__":
    from multiprocessing import Process, Queue
    import subprocess
    import signal
    import os

    binding_interface = binding_interface_client_t()

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

    openned = False

    def _on_open_done_cb(resp):
        assert not resp[0].startswith("ERROR"), "Failed to get serial"
        success = resp[1]
        assert success, "Failed to open serial"
        log(f"Opened")
        global openned
        openned = True

    debug_lines = None

    def _on_debug_began(resp):
        global debug_lines
        debug_lines = 0

    def _on_debug_end(resp):
        global debug_lines
        debug_lines = None

    def _on_get_debug_msg(resp):
        global debug_lines
        log(f"Debug:" + str(resp))
        debug_lines += 1

    linux_proc = subprocess.Popen(['../../build/penguin/firmware.elf'])

    log("Wait for Linux")
    while not os.path.exists("/tmp/osm/UART_DEBUG_slave"):
        time.sleep(0.1)

    binding_interface.open("/tmp/osm/UART_DEBUG_slave", _on_open_done_cb)

    log("Wait for open")
    while not openned:
        linux_proc.poll()
        binding_interface.process_message()
        time.sleep(0.1)

    binding_interface.get_serial_num(_on_get_serial_cb)
    binding_interface.get_fw_version(_on_get_fw_cb)
    binding_interface.change_sample("FW", 2)

    while binding_interface.process_message() != None:
        linux_proc.poll()
        time.sleep(0.1)

    binding_interface.unsolicited_handlers = {"DEBUG": _on_get_debug_msg}
    binding_interface.debug_begin(_on_debug_began)

    log("Waiting for debugmode")
    while debug_lines is None:
        linux_proc.poll()
        binding_interface.process_message()
        time.sleep(0.1)

    log("Reading debug")
    while debug_lines < 10:
        linux_proc.poll()
        binding_interface.process_message()
        time.sleep(0.1)

    binding_interface.debug_end(_on_debug_end)

    log(f"Waiting for debug stop")
    while debug_lines is not None:
        linux_proc.poll()
        binding_interface.process_message()
        time.sleep(0.1)

    log(f"Waiting for cleanup")
    binding_interface.finish()

    linux_proc.send_signal(signal.SIGINT)
    linux_proc.wait()
    log("Complete")
