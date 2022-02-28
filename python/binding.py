import serial
import datetime
import time
import csv
import sys
import io
import os
import re
import string
import random
import json


def default_print(msg):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
    print("[%s] %s\r"% (now, msg), file=sys.stderr)

_debug_fnc = lambda *args : default_print if "DEBUG" in os.environ else None


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


def parse_one_wire(r_str:str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return float(r[-1])
    return False


def parse_current_clamp(r_str:str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return int(r[-1])
    return False


def parse_particles(r_str:str):
    if "ERROR" in r_str:
        return False
    if "No HPM data." in r_str:
        return False
    if "HPM disabled" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return [int(r[-3]),int(r[-1])]
    return False


def parse_lora_comms(r_str:str):
    return "Connected" in r_str


class measurement_t(object):
    def __init__(self, name:str, type_:type, cmd:str, parse_func):
        self.name = name
        self.type_ = type_
        self.cmd = cmd
        self._value = None
        self.parse_func = parse_func

    @property
    def value(self):
        try:
            if self.type_ == bool:
                return bool(int(self._value) == 1)
            if self.type_ == int:
                return int(self._value)
            if self.type_ == float:
                return float(self._value)
            if self.type_ == str:
                return str(self._value)
        except ValueError:
            return False
        except TypeError:
            return False
        return self._value

    @value.setter
    def value(self, value_str):
        v_str = value_str.replace(self.cmd, "")
        self._value = self.parse_func(v_str)
        self.type_ = type(self._value)


class modbus_reg_t(measurement_t):
    def __init__(self, name:str, address:int, func:int, mb_type_:str, handle:str):
        self.name = name
        self.address = address
        self.func = func
        self.mb_type_ = mb_type_
        self.handle = handle
        self.type_ = float

    def parse_func(self, r_str):
        if "ERROR" in r_str:
            return False
        r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
        # r = re.findall(r'0x[0-9A-F]+', r_str)
        if r:
            return float(r[-1])
        return False

    @property
    def cmd(self)->str:
        return f"mb_get_reg {self.handle}"


class reader_child_t(object):
    def __init__(self, parent, measurement):
        self._parent = parent
        self._measurement = measurement

    @property
    def value(self):
        self._parent.get_val(self._measurement)
        return self._measurement.value


class log_t(object):
    def __init__(self, sender, receiver):
        self.sender = sender
        self.receiver = receiver

    def emit(self, payload):
        debug_print(payload)

    def send(self, msg):
        self.emit("%s >>> %s : %s"% (self.sender, self.receiver, msg))

    def recv(self, msg):
        self.emit("%s <<< %s : %s"% (self.sender, self.receiver, msg))


class low_level_dev_t(object):
    def __init__(self, serial_obj, log_obj):
        self._serial = io.TextIOWrapper(io.BufferedRWPair(serial_obj, serial_obj), newline="\r\n")
        self._log_obj = log_obj

    def write(self, msg):
        self._log_obj.send(msg)
        self._serial.write("%s\n"% msg)
        self._serial.flush()
        time.sleep(0.1)

    def read(self):
        try:
            msg = self._serial.readline().strip("\r\n")
        except UnicodeDecodeError:
            return None
        if msg == '':
            return None
        self._log_obj.recv(msg)
        return msg

    def readlines(self):
        msg = self.read()
        msgs = []
        while msg != None:
            msgs.append(msg)
            msg = self.read()
        return msgs


class dev_t(object):
    def __init__(self, port="/dev/ttyUSB0"):
        self._serial_obj = serial.Serial(port=port,
                                         baudrate=115200,
                                         bytesize=serial.EIGHTBITS,
                                         parity=serial.PARITY_NONE,
                                         stopbits=serial.STOPBITS_ONE,
                                         timeout=0,
                                         xonxoff=False,
                                         rtscts=False,
                                         write_timeout=None,
                                         dsrdtr=False,
                                         inter_byte_timeout=None,
                                         exclusive=None)
        self._log_obj = log_t("PC", "OSM")
        self._ll = low_level_dev_t(self._serial_obj, self._log_obj)

        self._children = {
            "w1"        : measurement_t("One Wire"           , float , "w1"        , parse_one_wire       ),
            "cc"        : measurement_t("Current Clamp"      , int   , "cc"        , parse_current_clamp  ),
            "hpm"       : measurement_t("Particles (2.5|10)" , str   , "hpm 1"     , parse_particles      ),
            "lora_conn" : measurement_t("LoRa Comms"         , bool  , "lora_conn" , parse_lora_comms     ),
            "temp"      : measurement_t("Temperature"        , float , "tmp"       , parse_temp           ),
            "humi"      : measurement_t("Humidity"           , float , "humi"      , parse_humidity       ),
            "PF"       : modbus_reg_t("Power Factor"    , 0x36, 4, "F", "PF"  ),
            "VP1"      : modbus_reg_t("Phase 1 volts"   , 0x00, 4, "F", "VP1" ),
            "VP2"      : modbus_reg_t("Phase 2 volts"   , 0x02, 4, "F", "VP2" ),
            "VP3"      : modbus_reg_t("Phase 3 volts"   , 0x04, 4, "F", "VP3" ),
            "AP1"      : modbus_reg_t("Phase 1 amps"    , 0x10, 4, "F", "AP1" ),
            "AP2"      : modbus_reg_t("Phase 2 amps"    , 0x12, 4, "F", "AP2" ),
            "AP3"      : modbus_reg_t("Phase 3 amps"    , 0x14, 4, "F", "AP3" ),
            "Imp"      : modbus_reg_t("Import Energy"   , 0x60, 4, "F", "Imp" ),
        }


    def __getattr__(self, attr):
        child = self._children.get(attr, None)
        if child:
            return reader_child_t(self, child)
        raise AttributeError


    def do_cmd(self, cmd:str, timeout:float=1.5)->str:
        self._ll.write(cmd)
        end_time = time.monotonic() + timeout
        r = []
        while time.monotonic() < end_time:
            r += self._ll.readlines()
            assert not "ERROR" in r, "OSM Error"
        r_str = ""
        for i in range(0, len(r)):
            line = r[i]
            r_str += str(line)
        return r_str

    def get_val(self, cmd:measurement_t):
        cmd.value = self.do_cmd(cmd.cmd)
        if cmd.value is False:
            time.sleep(4)
            cmd.value = self.do_cmd(cmd.cmd)

    def get_vals(self, cmds:list):
        for cmd in cmds:
            assert isinstance(cmd, measurement_t), "Commands should be of type measurement_t"
            cmd.value = self.do_cmd(cmd.cmd)
            if cmd.value is False:
                time.sleep(4)
                cmd.value = self.do_cmd(cmd.cmd)

    def _set_debug(self, value:int)->int:
        r = self.do_cmd(f"debug {hex(value)}")
        r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r)
        if r:
            return int(r[-1])

    def modbus_dev_add(self, slave_id:int, device:str)->bool:
        r = self.do_cmd(f"mb_dev_add {slave_id} {device}", timeout=3)
        return "Added modbus device" in r

    def modbus_reg_add(self, reg:modbus_reg_t)->bool:
        if not isinstance(reg, modbus_reg_t):
            self._log("Registers should be an object of register")
            return False
        self.do_cmd(f"mb_reg_add {slave_id} {hex(reg.address)} {reg.func} {reg.mb_type_} {reg.handle}")
        return "Added modbus reg" in r

    def setup_modbus_dev(self, slave_id:int, device:str, regs:list)->bool:
        if not self.modbus_dev_add(slave_id, device):
            self._log("Could not add device.")
            return False
        for reg in regs:
            self._ll.write(f"mb_reg_add {slave_id} {hex(reg.address)} {reg.func} {reg.mb_type_} {reg.handle}")
            self._ll.readlines()
        self._ll.write("mb_setup BIN 9600 8N1")
        self._ll.readlines()
        return True

    def modbus_dev_del(self, device:str):
        self._ll.write(f"mb_dev_del {device}")

    def current_clamp_calibrate(self):
        self._ll.write("cc_cal")
