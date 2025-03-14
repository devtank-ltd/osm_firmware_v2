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
import platform

from importlib import reload
from typing import List

MODBUS_REG_SET_ADDR_SUCCESSFUL_PATTERN = "Successfully set (?P<dev_name>.{1,4})\((?P<unit_id>0x[0-9]+)\):(?P<reg_addr>0x[0-9A-Fa-f]+) = (?P<type>(U16)|(I16)|(U32)|(I32)|(FLOAT)):(?P<value>[0-9]+.[0-9]+)"
MODBUS_REG_SET_NAME_SUCCESSFUL_PATTERN = "Successfully set (?P<dev_name>.{1,4})\((?P<unit_id>0x[0-9]+)\):(?P<reg_name>.{1,4})\((?P<reg_addr>0x[0-9A-Fa-f]+)\) = (?P<type>(U16)|(I16)|(U32)|(I32)|(FLOAT)):(?P<value>[0-9]+.[0-9]+)"

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


def parse_float(r_str: str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_osm_comms(r_str: str):
    return "Connected" in r_str


def parse_word(index: int, r_str: str):
    if index >= len(r_str):
        return ""
    try:
        return r_str.split()[index]
    except IndexError:
        return ""

class dev_child_t(object):
    def __init__(self, parent):
        self._parent = weakref.ref(parent)

    @property
    def parent(self):
        return self._parent()


class property_t(dev_child_t):
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
        if self._value is False and self.type_ != bool:
            raise IOError(f"Failed to get value for {self.name}")
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



class measurement_t(property_t):
    def __init__(self, parent, name: str, interval:int, samples:int, is_writable: bool = False, timeout: float = 2.0):
        super().__init__(parent, name, None, f"get_meas {name}", self._parse, is_writable, timeout)
        self._interval = interval
        self._samples = samples

    def _parse(self, r_str:str):
        if "ERROR" in r_str:
            return False
        r = re.search(f"{self.name}: ", r_str)
        if r is None:
            return False
        r_str = r_str[r.end():]
        if r_str[0] == '"':
            return r_str
        if re.match("^[0-9.+-]+$", r_str):
            return float(r_str)
        return r_str

    @property
    def interval(self):
        return self._interval

    @interval.setter
    def interval(self, v):
        self.parent.change_interval(self.name, v)
        self._interval = v

    @property
    def samplecount(self):
        return self._samples

    @samplecount.setter
    def samplecount(self, v):
        self.parent.change_samplec(self.name, v)
        self._samples = v


class modbus_reg_t(measurement_t):
    def __init__(self, parent, name: str, address: int, func: int, mb_type_: str, interval: int = 1, samples: int = 1, timeout: float = 2.0):
        super().__init__(parent, name, interval, samples, is_writable = False, timeout = timeout)
        self.address = address
        self.func = func
        self.mb_type_ = mb_type_

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


class comms_t(object):
    def __init__(self, parent):
        self._parent = weakref.ref(parent)

    @property
    def parent(self):
        return self._parent()

    def __getattr__(self, __name: str):
        config = self.COMMS_ATTR_DICT.get(__name)
        if config:
            return self._get_config(*config)
        return super().__getattribute__(__name)

    def __setattr__(self, __name: str, __value):
        config = self.COMMS_ATTR_DICT.get(__name)
        if config:
            self._set_config(config[0], __value)
            return
        super().__setattr__(__name, __value)

    def as_dict(self):
        return dict([ (name, self._get_config(*config))
                      for name, config in self.COMMS_ATTR_DICT.items() ])

    def from_dict(self, config_dict):
        for name, value in config_dict.items():
            config = self.COMMS_ATTR_DICT.get(name)
            if not config:
                raise IOError(f"No comms attribute {name}")
            self._set_config(config[0], value)

    def _get_config(self, cmd_name, pre):
        resp = self.parent.do_cmd_multi(f"comms_config {cmd_name}")
        if resp and resp[0].startswith(pre):
            return resp[0][len(pre):]
        return ""

    def _set_config(self, cmd_name, value):
        self.parent.do_cmd_multi(f"comms_config {cmd_name} {value}")

class lw_comms_t(comms_t):
    COMMS_ATTR_DICT = \
    {
        "dev_eui"  : ( "dev-eui", "Dev EUI: "  ),
        "app_key"  : ( "app-key", "App Key: "  )
    }

class wifi_comms_t(comms_t):
    COMMS_ATTR_DICT = \
    {
        "wifi_ssid" : ( "wifi_ssid", "SSID: " ),
        "wifi_pwd"  : ( "wifi_pwd",  "PWD: "  ),
        "mqtt_addr" : ( "mqtt_addr", "ADDR: " ),
        "mqtt_user" : ( "mqtt_user", "USER: " ),
        "mqtt_pwd"  : ( "mqtt_pwd",  "PWD: "  ),
        "mqtt_sch"  : ( "mqtt_sch",  "SCHEME: "),
        "mqtt_path" : ( "mqtt_path", "PATH: " ),
        "mqtt_port" : ( "mqtt_port", "PORT: " ),
        "schan"     : ( "schan",     "SCHAN: "   ),
        "nchan"     : ( "nchan",     "NCHAN: "   ),
        "country"   : ( "country",   "COUNTRY: " ),
    }

class poe_comms_t(comms_t):
    COMMS_ATTR_DICT = \
    {
        "mqtt_addr" : ( "mqtt_addr", "ADDR: " ),
        "mqtt_user" : ( "mqtt_user", "USER: " ),
        "mqtt_pwd"  : ( "mqtt_pwd",  "PWD: "  ),
        "mqtt_sch"  : ( "mqtt_sch",  "SCHEME: "),
        "mqtt_path" : ( "mqtt_path", "PATH: " ),
        "mqtt_port" : ( "mqtt_port", "PORT: " ),
    }

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
        self.system = False if platform.system() == "Windows" else True

    def write(self, msg):
        self._log_obj.send(msg)
        self._serial.write(("%s\n" % msg).encode())
        time.sleep(0.05)

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
        now = time.monotonic()
        end_time = now + timeout
        new_msg = None
        msgs = []
        while now < end_time:
            if self.system:
                r = select.select([self], [], [], end_time - now)
                if not r[0]:
                    debug_print("Lines timeout")
            # Should be echo of command
            new_msg = self.read()
            if new_msg is None:
                debug_print("NULL line read.")
            if new_msg:
                new_msg = new_msg.strip("\n\r")
            if new_msg != end_line:
                msgs += [new_msg]
            else:
                break
            now = time.monotonic()
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

    def activate_io(self, meas, rise, pull):
        #Enabling one wire or pulsecount e.g. "en_w1 1 U"
        self.parent.do_cmd(f"en_{meas} {self._index} {rise} {pull}")

    def disable_io(self):
        self.parent.do_cmd(f"io {self._index} : I N")

    def active_as(self):
        line = self.parent.do_cmd(f"io {self._index}")
        used_pos = line.find("USED")
        if used_pos == -1:
            return None
        pin_is = line[used_pos:].split()[1]
        if pin_is == "PLSCNT":
            if self._index == 1:
                return "CNT1"
            elif self._index == 2:
                return "CNT2"
        elif pin_is == "W1":
            if self._index == 1:
                return "TMP2"
            elif self._index == 2:
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

    def drain(self, timeout=1):
        while True:
            r = select.select([self],[],[],timeout)
            if r[0]:
                self._ll.read()
            else:
                break


class dev_t(dev_base_t):
    def __init__(self, port):
        super().__init__(port)
        self.drain(timeout=0.1)
        line = self.do_cmd("count")
        debug_print(f"LINE '{line}'")
        try:
            self._io_count = int(line.split()[-1])
        except IndexError:
            raise IndexError("Cannot query OSM, is it turned on?")
        self._children = {
            "ios"       : ios_t(self, self._io_count),
            "comms_conn": property_t(self,    "OSM Comms"         , bool  , "comms_conn"     , parse_osm_comms ),
            "version"   : property_t(self,    "FW Version"         , str   , "version"   , lambda s : parse_word(2, s) ),
            "serial_num": property_t(self,    "Serial Number"      , str   , "serial_num", lambda s : parse_word(2, s) ),
        }
        self.update_measurements()
        self.port = port
        self.comms_cfg = self.get_comms_config()
        self.comms = self._get_comms()

    def __getattr__(self, attr):
        child = self._children.get(attr, None)
        if child:
            return child
        self._log('No attribute "%s"' % attr)
        raise AttributeError

    def _log(self, msg):
        self._log_obj.emit(msg)

    def _get_comms(self):
        try:
            comms_type = self.comms_cfg["type"]
        except KeyError:
            self._log("Key 'type' not found")
            return None
        comms_types = { "LW"       : lambda s : lw_comms_t(s),
                        "AT WIFI"  : lambda s : wifi_comms_t(s),
                        "AT POE"   : lambda s : poe_comms_t(s)
                        }
        return comms_types.get(comms_type, lambda s: None)(self)

    def get_comms_config(self):
        comms_type = self.do_cmd("j_comms_cfg")
        comms_cfg = None
        try:
            comms_cfg = json.loads(comms_type)
        except Exception as e:
            self._log("Could not get comms config.")
            return None
        return comms_cfg

    @property
    def mac_address(self):
        try:
            mac = self.comms_cfg["config"]["HWID"]
        except KeyError:
            self._log("Could not get mac address")
            return None
        return mac

    @property
    def name(self):
        r_str = self.do_cmd("name")
        m = re.match("Name: (?P<osm_name>.*)$", r_str)
        if not m:
            # Fallback to random name and saved config can be updated later.
            return "random-" + app_key_generator(8)
        return m.groupdict()["osm_name"]

    def set_name(self, new_name):
        return self.do_cmd(f"name {new_name}")

    def set_serial_num(self, serial_num):
        self.do_cmd("serial_num %s" % serial_num)

    def enable_pulsecount(self, count:int):
        self.do_cmd(f"en_pulse {count} R U")

    def send_alive_packet(self):
        return self.do_cmd("connect")

    def reconnect_announce(self, timeout_s:float=30., type_="lw") -> str | bool:
        comms_id = None
        self.set_dbg(4)
        self.do_cmd("comms_restart")
        end_time = time.monotonic() + timeout_s
        connected = self.comms_conn.value
        while not connected:
            if time.monotonic() > end_time:
                self._log("Timeout reached, could not confirm send.")
                return False
            time_left = end_time - time.monotonic()
            lines = self._ll.readlines()
            comms_id = self.parse_comms_id(lines)
            sleep_time = 0.5 if time_left > 0.5 else time_left
            time.sleep(sleep_time)
            connected = self.comms_conn.value
        if type_ == "lw":
            self.send_alive_packet()
        return comms_id

    def parse_comms_id(self, lines: str) -> str | None:
        comms_id = None
        comms_id_pattern = 'DEBUG:[0-9]{10}:COMMS:  << AT\+MQTTSUB=0,"osm/(?P<comms_id>[0-9A-F]{8})/cmd",0'
        for line in lines:
            r = re.search(comms_id_pattern, line)
            if not r:
                continue
            try:
                comms_id = r.groupdict()["comms_id"]
                break
            except KeyError:
                self._log("Key doesnt exist: comms_id")
        return comms_id

    def get_rak_version(self):
        self.set_dbg(4)
        self.do_cmd("comms_restart")
        comms = self._ll.readlines(timeout=1)
        for line in comms:
            if "4.1.0" in line:
                self.set_dbg()
                return True
        self.set_dbg()
        return False

    def set_dbg(self, mode:int=0):
        return self.do_cmd(f"debug {mode}")

    @property
    def interval_mins(self):
        r = self.do_cmd_multi("interval_mins")
        rs = float(r[0].split()[-1])
        return float(r[0].split()[-1])

    @interval_mins.setter
    def interval_mins(self, value):
        return self.do_cmd_multi("interval_mins %f" % value)

    def set_tx_power(self, val: int):
        if val >= 0 and val <= 7:
            self.do_cmd(f"comms_txpower {val}")
        else:
            self._log("Value must be an integer between 0 and 7.")


    def change_samplec(self, meas, val):
        self.do_cmd(f"samplecount {meas} {val}")

    def change_interval(self, meas, val):
        self.do_cmd(f"interval {meas} {val}")

    def get_measurement_handles(self):
        meas = self.measurements()
        print(meas)

    def get_ftma_types(self, headers):
        list_of_ftma = []
        for head in headers[1]:
            ftma = self.do_cmd(f"get_meas_type {head}")
            s = ftma.split(': ')
            if s[1] == "FTMA":
                list_of_ftma.append(s[0])
        return list_of_ftma

    def set_ftma_name(self, name, meas):
        return self.do_cmd(f"ftma_name {name} {meas}")

    def get_meas_timeout(self, meas):
        line = self.do_cmd(f"get_meas_to {meas}")
        if line.startswith(meas):
            parts = line.split(":")
            if len(parts) == 2:
                to = int(parts[1]) / 500
                r = max(1.5, to)
                debug_print(f"Measurement {meas} has timeout {r}")
                return r
        return None

    def activate_io(self, meas, pin, pull):
        #Enabling one wire or pulsecount e.g. "en_w1 4 U"
        self.do_cmd(f"{meas} {pin} {pull}")

    def disable_io(self, pin):
        self.do_cmd(f"io {pin} : I N")

    def cc_type(self):
        return self.do_cmd_multi("cc_gain")

    @property
    def print_cc_gain(self):
        return self.do_cmd_multi("cc_gain")

    def cc_midpoint(self, phase: str) -> float | None:
        mp = self.do_cmd(f"cc_mp {phase}")
        mp_re = re.findall("\d+[\.\d+]?", mp)
        if not mp_re:
            return None
        return float("".join(mp_re))

    def set_cc_midpoint(self, value, phase):
        return self.do_cmd_multi(f"cc_mp {value} {phase}")

    def set_input_output_cc(self, phase, input, output):
        self.do_cmd(f"cc_gain {phase} {input} {output}")

    def set_cc_type(self, phase, ctype):
        phases = [1,2,3]
        units = ['A', 'V']
        if phase not in phases or ctype not in units:
            debug_print('Invalid arguments supplied.')
            return
        return self.do_cmd(f"cc_type {phase} {ctype}")

    def save(self):
        return self.do_cmd("save")

    def do_cmd(self, cmd: str, timeout: float = 1.5) -> str:
        r = self.do_cmd_multi(cmd, timeout)
        if r is None:
            return ""
        return "".join([str(line) for line in r])

    def do_cmd_multi(self, cmd: str, timeout: float = 1.5) -> List[str] | None:
        self._ll.write(cmd)
        debug_print(f"Reading with Timeout : {timeout}")
        now = time.monotonic()
        end_time = now + timeout
        r = self._ll.readlines(_RESPONSE_END, end_time - now)
        start_pos = None
        for n in range(0, len(r)):
            line = r[n]
            if line == _RESPONSE_BEGIN:
                start_pos = n+1
        if start_pos is not None:
            return r[start_pos:]
        debug_print("No response start found.")
        return None

    def read_ftma_coeffs(self, meas):
        coeffs = self.do_cmd(f"ftma_coeff {meas}")
        print(coeffs)
        return coeffs

    def measurements(self):
        r = self.do_cmd_multi("measurements")
        meas_list = []
        if r:
            r = r[2:]
            meas_list = [line.replace("\t\t", "\t").split("\t") for line in r]
        return meas_list

    def update_measurements(self):
        new_chldren = {}
        for key, child in self._children.items():
            if not isinstance(child, measurement_t):
                new_chldren[key] = child
        self._children = new_chldren
        r = self.measurements()
        assert r
        if r[0] == ['Name', 'Interval', 'Sample Count']:
            r = r[1:]
        for line in r:
            name, interval, sample_count = line
            interval=int(interval.split('x')[0])
            sample_count=int(sample_count)
            new_measurement = measurement_t(self, name, interval, sample_count)
            new_measurement.timeout = self.get_meas_timeout(name)
            self._children[name] = new_measurement

    def measurements_enable(self, value):
        self.do_cmd("meas_enable %s" % ("1" if value else "0"))

    def set_ftma_coeffs(self, a, b, c, d, meas):
        self.do_cmd(f"ftma_coeff {meas} {a} {b} {c} {d}")

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
                        reg[0][2:], 16), mb_type_=reg[-1], func=int(reg[1][3:4]))
                    reg.timeout = self.get_meas_timeout(name)
                    dev["regs"] += [reg]
        else:
            pass

        return mb_bus_t(config=bus_config, devices=[mb_dev_t(**dev) for dev in devs])

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
            f"mb_reg_add {slave_id} {hex(reg.address)} {reg.func} {reg.mb_type_} {reg.name}")
        if r:
            reg.timeout = self.get_meas_timeout(reg.name)
        self._children[reg.name] = reg
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
            self._children[reg.name] = reg
        return True

    def modbus_dev_del(self, device: str):
        self.do_cmd(f"mb_dev_del {device}")

    def modbus_reg_del(self, reg: str):
        self.do_cmd(f"mb_reg_del {reg}")

    def modbus_reg_set(self, **kwargs):
        """
        mb_reg_set <reg_name> <value>
        mb_reg_set <device_name> <reg_addr> <type> <value>
        """
        reg     = kwargs.get("reg",     None)
        dev     = kwargs.get("dev",     None)
        value   = kwargs.get("value",   0.  )
        timeout = kwargs.get("timeout", 5.  )
        if dev:
            reg_addr = kwargs["reg_addr"]
            type_    = kwargs["type"]
        if dev is None == reg is None:
            raise Error("Only dev or reg must be given to set the register.")
        if dev:
            cmd = f"mb_reg_set {dev} {hex(reg_addr)} {type_} {value}"
        else:
            cmd = f"mb_reg_set {reg} {value}"
        resp = self.do_cmd(cmd, timeout=timeout)
        if dev:
            match_ = re.search(MODBUS_REG_SET_ADDR_SUCCESSFUL_PATTERN, resp)
        else:
            match_ = re.search(MODBUS_REG_SET_NAME_SUCCESSFUL_PATTERN, resp)
        if not match_:
            return False
        resp_data = match_.groupdict()
        if dev:
            if resp_data["dev_name"] != dev:
                debug_print("Device name does not match.")
                return False
            if int(resp_data["reg_addr"], 16) != int(reg_addr):
                debug_print("Register address does not match.")
                return False
        else:
            if resp_data["reg_name"] != reg:
                debug_print("Register name is different to set.")
                return False
        # Maybe a better way of comparing floats.
        if float(resp_data["value"]) - value > 0.01:
            debug_print("Not set to same value.")
            return False
        # Could also compare stored values i.e. unit ID
        return True

    def current_clamp_calibrate(self):
        self.do_cmd("cc_cal")

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

    def reset(self, timeout=5):
        self._ll.write('reset')
        end_time = time.time() + timeout
        while time.time() < end_time:
            r = select.select([self],[],[], 1)
            if len(r[0]):
                line = self._ll.read()
                if '----start----' in line:
                    debug_print("OSM reset'ed")
                    return True
        return False

    def program_firmware(self, firmware:str, is_rev_c:bool=True, keep_config:bool=False)->bool:
        if not os.path.exists(firmware):
            self._log(f"{firmware} does not exist.")
            return False
        dev_name = os.path.basename(str(self.port))
        self._serial_obj.close()
        env = {"dev": dev_name}
        if is_rev_c:
            env["REVC"] = "1"
        if keep_config:
            env["KEEPCONFIG"] = "1"
        this_file = os.path.abspath(__file__)
        this_dir = os.path.dirname(this_file)
        cmd = f"{this_dir}/tools/config_scripts/program.sh {firmware}"
        cmd = cmd.split(" ")
        proc = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        events = {
            proc.stdout : "STDOUT",
            proc.stderr : "STDERR"}
        while proc.poll() is None:
            r = select.select(list(events.keys()), [], [], 0.05)
            sources = r[0]
            for stream in sources:
                pretext = events.get(stream, None)
                msg = stream.readline()
                if len(msg):
                    if pretext is not None:
                        try:
                            msg = msg.decode()
                        except UnicodeDecodeError:
                            pass
                        self._log(f"{pretext}: {msg}")
                    else:
                        self._log(f"Unknown pretext for {stream}")

        retcode = proc.returncode
        if retcode == 0:
            self._log(f"Finished programming with return code: {retcode}")
        else:
            self._log(f"Programming exited with return code: {retcode}")

        self._serial_obj.open()
        return retcode == 0


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
            "DEBUG:[0-9]{10}:DEBUG:[A-Za-z0-9]+:[U8|U16|U32|U64|I8|I16|I32|I64|F|i64|f32|str]+:[0-9]+(\.[0-9]+)?", msg)
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
                    p_arr.append((line, p))
            if p_arr:
                return p_arr
