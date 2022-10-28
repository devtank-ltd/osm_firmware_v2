from imp import reload
import threading
import serial
import select
import datetime
import time
import csv
import sys
import io
import os
import re
import weakref
import string
import random
import json


_RESPONSE_BEGIN = "============{"
_RESPONSE_END = "}============"


def app_key_generator(size=32, chars=string.hexdigits):
    """ Technically this will do upper and lowercase letters so the
    randomness is skewed towards letters. """
    return ''.join(random.choice(chars) for _ in range(size))


def dev_eui_generator(size=16, chars=string.hexdigits):
    """ Technically this will do upper and lowercase letters so the
    randomness is skewed towards letters. """
    return ''.join(random.choice(chars) for _ in range(size))


def default_print(msg):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
    print("[%s] %s" % (now, msg), file=sys.stderr)


_debug_fnc = default_print if "DEBUG" in os.environ else None


def debug_print(msg):
    if _debug_fnc:
        _debug_fnc(msg)


def set_debug_print(_func):
    global _debug_fnc
    _debug_fnc = _func


def get_debug_print():
    return _debug_fnc


def parse_temp(r_str: str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_humidity(r_str: str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_light(r_str: str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_one_wire(r_str: str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_current_clamp(r_str: str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_particles(r_str: str):
    if "ERROR" in r_str:
        return False
    if "No HPM data." in r_str:
        return False
    if "HPM disabled" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return (int(r[-3]), int(r[-1]))
    return False


def parse_lora_comms(r_str: str):
    return "Connected" in r_str


def parse_word(index: int, r_str: str):
    if index >= len(r_str):
        return ""
    return r_str.split()[index]


class dev_child_t(object):
    def __init__(self, parent):
        self._parent = weakref.ref(parent)

    @property
    def parent(self):
        return self._parent()
    

class measurement_t(dev_child_t):
    def __init__(self, parent, name: str, type_: type, cmd: str, parse_func, is_writable: bool = False, timeout: float = 1.0):
        super().__init__(parent)
        self.timeout = timeout
        self.name = name
        self.type_ = type_
        self.cmd = cmd
        self._value = None
        self.parse_func = parse_func
        self.is_writable = is_writable

    def _update(self, value_str):
        v_str = value_str.replace(self.cmd, "")
        self._value = self.parse_func(v_str)
        self.type_ = type(self._value)

    @property
    def value(self):
        r = self.parent.do_cmd(self.cmd, self.timeout)
        self._update(r)
        try:
            if self.type_ == bool:
                return bool(int(self._value) == 1)
            return self.type_(self._value)
        except ValueError:
            return False
        except TypeError:
            return False
        return self._value

    @value.setter
    def value(self, v):
        assert isinstance(v, cmd.type_)
        assert self.is_writable
        if self.type_ == bool:
            v = 1 if v else 0
        self.parent.do_cmd(f"{cmd.cmd} {v}")
        self._update(v)


class hpm_t(measurement_t):
    def __init__(self, parent):
        super().__init__(parent, "Particles (2.5|10)", str, "hpm 1", parse_particles)
        self._enabled = False

    @property
    def enabled(self):
        return self._enabled

    @enabled.setter
    def enabled(self, enabled:bool):
        v = 1 if enabled else 0
        self.parent.do_cmd(f"hpm {v}")
        self._enabled = enabled


class modbus_reg_t(measurement_t):
    def __init__(self, parent, name: str, address: int, func: int, mb_type_: str, handle: str):
        super().__init__(parent, name, float, f"mb_get_reg {handle}", self._parse_func, False, 2.0)
        self.address = address
        self.func = func
        self.mb_type_ = mb_type_
        self.handle = handle

    def _parse_func(self, r_str):
        if "ERROR" in r_str:
            return False
        r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
        # r = re.findall(r'0x[0-9A-F]+', r_str)
        if r:
            return float(r[-1])
        return False

    def __str__(self):
        return f'{str(self.name)}, {str(self.address)}, {str(self.mb_type_)}, {str(self.func)}'


class mb_dev_t(object):
    def __init__(self, name, unit_id, regs, byteorder, wordorder):
        self.unit = unit_id
        self.name = name
        self.regs = regs
        self.byteorder = byteorder
        self.wordorder = wordorder

    def __str__(self):
        return f'{str(self.unit)}, {str(self.name)}, {str(self.regs)}, {str(self.byteorder)}, {str(self.wordorder)}'


class mb_bus_t(object):
    def __init__(self, config, devices):
        self.config = config
        self.devices = devices

    def __str__(self):
        return f'{self.config}, {self.devices}'


class log_t(object):
    def __init__(self, sender, receiver):
        self.sender = sender
        self.receiver = receiver

    def emit(self, payload):
        debug_print(payload)

    def send(self, msg):
        self.emit("%s >>> %s : %s" % (self.sender, self.receiver, msg))

    def recv(self, msg):
        self.emit("%s <<< %s : %s" % (self.sender, self.receiver, msg))


class low_level_dev_t(object):
    def __init__(self, serial_obj, log_obj):
        self._serial = serial_obj
        self._log_obj = log_obj
        self.fileno = serial_obj.fileno

    def write(self, msg):
        self._log_obj.send(msg)
        self._serial.write(("%s\n" % msg).encode())
        self._serial.flush()
        time.sleep(0.3)

    def read(self):
        try:
            msg = self._serial.readline().decode()
        except UnicodeDecodeError:
            return None
        if msg == '':
            return None
        if len(msg) == 0:
            return None
        self._log_obj.recv(msg.strip("\n\r"))
        return msg

    def readlines(self, end_line=None, timeout=1):
        r = select.select([self], [], [], timeout)
        if not r[0]:
            debug_print("Lines timeout")
            return []
        new_msg = self.read()
        msgs = []
        if new_msg is None:
            return msgs

        while new_msg != end_line:
            new_msg = new_msg.strip("\n\r")
            msgs += [new_msg]
            new_msg = self.read()
            if new_msg is None:
                return msgs

        return msgs


class io_t(dev_child_t):
    def __init__(self, parent, index):
        super().__init__(parent)
        self._index = index

    @property
    def value(self):
        return self.parent.get_io(self._index)

    @value.setter
    def value(self, v:bool):
        self.parent.set_io(self._index, v)

    def configure(self, is_input: bool, bias: str):
        self.parent.configure_io(self._index, is_input, bias)

    def activate_io(self, meas, pull):
        #Enabling one wire or pulsecount e.g. "en_w1 4 U"
        self.parent.do_cmd(f"en_{meas} {self._index} {pull}")

    def disable_io(self):
        self._parent.do_cmd(f"io {self._index} : I N")

    def active_as(self):
        line = self.parent.do_cmd(f"io {self._index}")
        used_pos = line.find("USED")
        if used_pos == -1:
            return None
        pin_is = line[used_pos:].split()[1]
        if pin_is == "PLSCNT":
            if self._index == 4:
                return "CNT1"
            elif self._index == 5:
                return "CNT2"
        elif pin_is == "W1":
            if self._index == 4:
                return "TMP2"
            elif self._index == 5:
                return "TMP3"

    def active_pull(self):
        line = self.parent.do_cmd(f"io {self._index}")
        used_pos = line.find("USED")
        if used_pos == -1:
            return None
        return line[used_pos:].split()[2]


class ios_t(dev_child_t):
    def __init__(self, parent, count):
        super().__init__(parent)
        self._count = count

    def __len__(self):
        return self._count

    def __getitem__(self, index):
        if index > self._count or index < 0:
            raise IndexError('IO index out of range')
        return io_t(self.parent, index)


class dev_base_t(object):
    def __init__(self, port):
        if isinstance(port, str):
            self._serial_obj = serial.Serial(port=port,
                                             baudrate=115200,
                                             bytesize=serial.EIGHTBITS,
                                             parity=serial.PARITY_NONE,
                                             stopbits=serial.STOPBITS_ONE,
                                             timeout=0.1,
                                             xonxoff=False,
                                             rtscts=False,
                                             write_timeout=None,
                                             dsrdtr=False,
                                             inter_byte_timeout=None,
                                             exclusive=None)
        elif isinstance(port, serial.Serial):
            self._serial_obj = port
        else:
            raise Exception("Unsupport serial port argument.")

        self._log_obj = log_t("PC", "OSM")
        self._log = self._log_obj.emit
        self._ll = low_level_dev_t(self._serial_obj, self._log_obj)
        self.fileno = self._ll.fileno


class dev_t(dev_base_t):
    def __init__(self, port):
        super().__init__(port)
        line = self.do_cmd("count")
        debug_print(f"LINE '{line}'")
        self._io_count = int(line.split()[-1])
        self._children = {
            "ios"       : ios_t(self, self._io_count),
            "w1"        : measurement_t(self, "One Wire"           , float , "w1 TMP2"   , parse_one_wire       ),
            "cc"        : measurement_t(self, "Current Clamp"      , int   , "cc"        , parse_current_clamp  ),
            "hpm"       : hpm_t(self),
            "comms_conn": measurement_t(self, "LoRa Comms"         , bool  , "comms_conn" , parse_lora_comms     ),
            "temp"      : measurement_t(self, "Temperature"        , float , "temp"      , parse_temp           ),
            "humi"      : measurement_t(self, "Humidity"           , float , "humi"      , parse_humidity       ),
            "light"     : measurement_t(self, "Light"              , float , "light"     , parse_light          , False, 10),
            "version"   : measurement_t(self, "FW Version"         , str   , "version"   , lambda s : parse_word(2, s) ),
            "serial_num": measurement_t(self, "Serial Number"      , str   , "serial_num", lambda s : parse_word(2, s) ),
        }
        self.update_modbus_registers()

    def __getattr__(self, attr):
        child = self._children.get(attr, None)
        if child:
            return child
        self._log('No attribute "%s"' % attr)
        raise AttributeError

    def _log(self, msg):
        self._log_obj.emit(msg)

    @property
    def interval_mins(self):
        r = self.do_cmd_multi("interval_mins")
        return int(r[0].split()[-1])

    @interval_mins.setter
    def interval_mins(self, value):
        return self.do_cmd("interval_mins %u" % value)

    @property
    def app_key(self):
        ak = self.do_cmd_multi("comms_config app-key")
        prec = "App Key: "
        if ak and ak[0].startswith(prec):
            return ak[0][len(prec):]
        return ""

    @app_key.setter
    def app_key(self, key):
        self.do_cmd_multi(f"comms_config app-key {key}")

    @property
    def dev_eui(self):
        de = self.do_cmd_multi("comms_config dev-eui")
        prec = "Dev EUI: "
        if de and de[0].startswith(prec):
            return de[0][len(prec):]
        return ""

    @dev_eui.setter
    def dev_eui(self, eui):
        self.do_cmd_multi(f"comms_config dev-eui {eui}")
    
    def change_samplec(self, meas, val):
        self.do_cmd(f"samplecount {meas} {val}")
    
    def change_interval(self, meas, val):
        self.do_cmd(f"interval {meas} {val}")
    
    def activate_io(self, meas, pin, pull):
        #Enabling one wire or pulsecount e.g. "en_w1 4 U"
        self.do_cmd(f"{meas} {pin} {pull}")
    
    def disable_io(self, pin):
        self.do_cmd(f"io {pin} : I N")
    
    @property
    def print_cc_gain(self):
        return self.do_cmd_multi("cc_gain")
    
    def get_midpoint(self, phase):
        return self.do_cmd_multi(f"cc_mp {phase}")
    
    def update_midpoint(self, value, phase):
        return self.do_cmd_multi(f"cc_mp {value} {phase}")
    
    def set_outer_inner_cc(self, phase, outer, inner):
        self.do_cmd(f"cc_gain {phase} {outer} {inner}")
    
    def save(self):
        self.do_cmd("save")
        
    def get_modbus_val(self, val_name, timeout: float = 0.5):
        self._ll.write(f"mb_get_reg {val_name}")
        end_time = time.monotonic() + timeout
        last_line = ""
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            if not new_lines:
                continue
            new_lines[0] = last_line + new_lines[0]
            last_line = new_lines[-1]
            for line in new_lines:
                if (f"Modbus: reg:{val_name}") in line:
                    try:
                        return int(line.split(':')[-1])
                    except ValueError:
                        pass
        return False

    def extract_val(self, cmd, val_name, timeout: float = 0.5):
        if cmd:
            self._ll.write(cmd)
        end_time = time.monotonic() + timeout
        last_line = ""
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            if not new_lines:
                continue
            new_lines[0] = last_line + new_lines[0]
            last_line = new_lines[-1]
            for line in new_lines:
                if ("Sound = ") in line:
                    try:
                        regex = re.findall(r"\d+\.", line)
                        new_r = regex[0]
                        new_str = new_r[:-1]
                        return int(new_str)
                    except ValueError:
                        pass
                if ("Lux: ") in line:
                    try:
                        return int(line.split(':')[-1])
                    except ValueError:
                        pass
                elif ("PM25:") in line and val_name == "PM25":
                    try:
                        pm_values = re.split('[:,]', line)
                        return int(pm_values[1])
                    except ValueError:
                        pass
                elif ("PM10:") in line:
                    try:
                        pm_values = re.split('[:,]', line)
                        return int(pm_values[3])
                    except ValueError:
                        pass
                elif ("Temperature") in line:
                    try:
                        temp = re.findall(r"\d+", line)
                        temp_float = '.'.join(temp)
                        return float(temp_float)
                    except ValueError:
                        pass
                elif ("Humidity") in line:
                    try:
                        humi = re.findall(r"\d+", line)
                        humi_float = '.'.join(humi)
                        return float(humi_float)
                    except ValueError:
                        pass
                elif val_name in line and "CC" in line:
                    try:
                        cc = line.split()[2]
                        return float(cc)
                    except ValueError:
                        pass
                elif ("onnected") in line:
                    try:
                        return int(line[0])
                    except ValueError:
                        pass
                elif ("CNT1") in line and val_name == "CNT1":
                    try:
                        cnt = line.split()[-1]
                        return int(cnt)
                    except ValueError:
                        pass
                elif ("CNT2") in line:
                    try:
                        cnt2 = line.split()[-1]
                        return int(cnt2)
                    except ValueError:
                        pass
                elif ("Temp") in line and val_name == "TMP2":
                    s = re.findall('\d', line)
                    v = ''.join(s)
                    try:
                        temp = v
                        return int(temp)
                    except ValueError:
                        pass
        return False

    def imp_readlines(self, timeout: float = 0.5):
        new_lines = self._ll.readlines()
        return "".join([str(line)+"\n" for line in new_lines])

    def dbg_readlines(self):
        return self._ll.readlines()

    def do_debug(self, cmd):
        if cmd is not None:
            self.do_cmd(cmd)

    def do_cmd(self, cmd: str, timeout: float = 1.5) -> str:
        r = self.do_cmd_multi(cmd, timeout)
        if r is None:
            return ""
        return "".join([str(line) for line in r])

    def do_cmd_multi(self, cmd: str, timeout: float = 1.5) -> str:
        self._ll.write(cmd)
        end_time = time.monotonic() + timeout
        r = []
        done = False
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines(_RESPONSE_END)
            for line in new_lines:
                if _RESPONSE_END in line:
                    done = True
            r += new_lines
            if done:
                break
            assert not "ERROR" in r, "OSM Error"
        start_pos = None
        end_pos = None
        for n in range(0, len(r)):
            line = r[n]
            if line == _RESPONSE_BEGIN:
                start_pos = n+1
            elif line == _RESPONSE_END:
                end_pos = n
        if start_pos is not None and end_pos is not None:
            return r[start_pos:end_pos]
        return None

    def measurements(self, cmd):
        r = self.do_cmd_multi(cmd)
        meas_list = []
        if r:
            r = r[2:]
            meas_list = [line.replace("\t\t", "\t").split("\t") for line in r]
        return meas_list

    def measurements_enable(self, value):
        self.do_cmd("meas_enable %s" % ("1" if value else "0"))

    def get_modbus(self):
        lines = self.do_cmd_multi("mb_log")
        bus_config = None
        dev = None
        devs = []

        if lines:
            for line in lines:
                if line.startswith("Modbus"):
                    bus_config = line.split()[2:]
                elif line.startswith("- Device"):
                    unit_id, name, byteorder, wordorder = line.split()[3:]
                    dev = {"unit_id": int(unit_id[2:], 16),
                           "name": name.strip('"'),
                           "byteorder": byteorder,
                           "wordorder": wordorder,
                           "regs": []}
                    devs += [dev]
                elif line.startswith("  - Reg"):
                    reg = line.split()[3:]
                    name = reg[2].strip('"')
                    reg = modbus_reg_t(self, name=name, address=int(
                        reg[0][2:], 16), mb_type_=reg[-1], func=int(reg[1][3:4]), handle=name)
                    dev["regs"] += [reg]
        else:
            pass

        return mb_bus_t(config=bus_config, devices=[mb_dev_t(**dev) for dev in devs])

    def update_modbus_registers(self):
        new_chldren = {}
        for key, child in self._children.items():
            if not isinstance(child, modbus_reg_t):
                new_chldren[key] = child
        self._children = new_chldren
        mb_root = self.get_modbus()
        for device in mb_root.devices:
            for reg in device.regs:
                self._children[reg.handle] = reg

    def get_io(self, index: int) -> bool:
        line = self.do_cmd("io %u" % index)
        state = line.split()[-1]
        return state == "ON"

    def set_io(self, index: int, enabled: bool):
        self.do_cmd("io %u = %u" % (index, int(enabled)))

    def configure_io(self, index: int, is_input: bool, bias: str):
        assert bias in ["U","N","D"], "Invalid IO bais"
        self.do_cmd("io %u : %s %s" % (index, "I" if is_input else "O", bias))

    def _set_debug(self, value: int) -> int:
        r = self.do_cmd(f"debug {hex(value)}")
        r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r)
        if r:
            return int(r[-1])

    def modbus_dev_add(self, slave_id: int, device: str, is_msb: bool, is_msw: bool) -> bool:
        is_msb = "MSB" if is_msb else "LSB"
        is_msw = "MSW" if is_msw else "LSW"
        r = self.do_cmd(f"mb_dev_add {slave_id} {is_msb} {is_msw} {device}", timeout=3)
        return "Added modbus device" in r

    def modbus_reg_add(self, slave_id: int, reg: modbus_reg_t) -> bool:
        if not isinstance(reg, modbus_reg_t):
            self._log("Registers should be an object of register")
            return False
        r = self.do_cmd(
            f"mb_reg_add {slave_id} {hex(reg.address)} {reg.func} {reg.mb_type_} {reg.handle}")
        self._children[reg.handle] = reg
        return "Added modbus reg" in r

    def setup_modbus(self, is_bin=False, baudrate=9600, bits=8, parity='N', stopbits=1):
        is_bin = "BIN" if is_bin else "RTU"
        self.do_cmd(f"mb_setup {is_bin} {baudrate} {bits}{parity}{stopbits}")

    def setup_modbus_dev(self, slave_id: int, device: str, is_msb: bool, is_msw: bool, regs: list) -> bool:
        if not self.modbus_dev_add(slave_id, device, is_msb, is_msw):
            self._log("Could not add device.")
            return False
        for reg in regs:
            self.modbus_reg_add(slave_id, reg)
            self._children[reg.handle] = reg
        return True

    def modbus_dev_del(self, device: str):
        self._ll.write(f"mb_dev_del {device}")
    
    def modbus_reg_del(self, reg: str):
        self._ll.write(f"mb_reg_del {reg}")

    def current_clamp_calibrate(self):
        self._ll.write("cc_cal")

    def write_lora(self):
        app_key = app_key_generator()
        return app_key

    def write_eui(self):
        dev_eui = dev_eui_generator()
        return dev_eui

    def fw_upload(self, filmware):
        from crccheck.crc import CrcModbus
        data = open(filmware, "rb").read()
        crc = CrcModbus.calc(data)
        hdata = ["%02x" % x for x in data]
        hdata = "".join(hdata)
        mtu = 56
        for n in range(0, len(hdata), mtu):
            debug_print("Chunk %u/%u" % (n/2, len(hdata)/2))
            chunk = hdata[n:n + mtu]
            r = self.do_cmd("fw+ "+chunk)
            expect = "FW %u chunk added" % (len(chunk)/2)
            assert expect in r
        r = self.do_cmd("fw@ %04x" % crc)
        assert "FW added" in r

    def reset(self):
        self._ll.write('reset')
        while True:
            line = self._ll.read()
            if '----start----' in line:
                return


class dev_debug_t(dev_base_t):
    def __init__(self, port):
        super().__init__(port)
        self._leftover = ""

    def parse_msg(self, msg):
        """
        IN:  'DEBUG:0000036805:DEBUG:TEMP:2113'
        OUT: ('TEMP', 2113)
        """
        r = re.search("DEBUG:[0-9]{10}:DEBUG:[A-Za-z0-9]+:FAILED", msg)
        if r and r.group(0):
            _, ts, _, name, _ = r.group(0).split(":")
            self._log(f"{name} failed.")
            return (name, False)
        r = re.search(
            "DEBUG:[0-9]{10}:DEBUG:[A-Za-z0-9]+:[U8|U16|U32|U64|I8|I16|I32|I64|F|i64]+:[0-9]+", msg)
        if r and r.group(0):
            _, ts, _, name, type_, value = r.group(0).split(":")
            try:
                value = float(value)
            except ValueError:
                self._log(f"{value} is not a float.")
                return None
            self._log(f"{name} = {value}")
            return (name, float(value))
        # Cannot find regular expression matching template
        return None

    def parse_msgs(self, msgs):
        resp = []
        for msg in msgs:
            resp.append(self.parse_msg(msg))
        return resp

    def read_msgs(self, timeout=1.5):
        end_time = time.monotonic() + timeout
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            if len(new_lines) == 0:
                continue
            new_lines[0] = (self._leftover + new_lines[0]).strip("\n\r")
            self._leftover = new_lines[-1]
            p_arr = []
            for line in new_lines:
                p = self.parse_msg(line)
                if p:
                    p_arr.append(p)
            if p_arr:
                return p_arr
