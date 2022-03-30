import serial
import datetime
import time
import csv
import sys
import io
import os
import re


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
        return self._measurement .value


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
        self.fileno = serial_obj.fileno

    def write(self, msg):
        self._log_obj.send(msg)
        self._serial.write("%s\n"% msg)
        self._serial.flush()
        time.sleep(0.1)

    def read(self):
        try:
            msg = self._serial.readline()
        except UnicodeDecodeError:
            return None
        if msg == '':
            return None
        if len(msg) == 0:
            return None
        self._log_obj.recv(msg.strip("\n\r"))
        return msg

    def readlines(self):
        new_msg = self.read()
        msg = ""
        while new_msg != None:
            msg += new_msg
            new_msg = self.read()
        msgs = msg.split("\n\r")
        if len(msgs) == 1 and msgs[0] == '':
            return []
        return msgs


class dev_base_t(object):
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
        self._log = self._log_obj.emit
        self._ll = low_level_dev_t(self._serial_obj, self._log_obj)
        self.fileno = self._ll.fileno


class dev_t(dev_base_t):
    def __init__(self, port="/dev/ttyUSB0"):
        super().__init__(port)
        self._children = {
            "w1"        : measurement_t("One Wire"           , float , "w1"        , parse_one_wire       ),
            "cc"        : measurement_t("Current Clamp"      , int   , "cc"        , parse_current_clamp  ),
            "hpm"       : measurement_t("Particles (2.5|10)" , str   , "hpm 1"     , parse_particles      ),
            "lora_conn" : measurement_t("LoRa Comms"         , bool  , "lora_conn" , parse_lora_comms     ),
            "PF"        : modbus_reg_t("Power Factor"         , 0xc56e, 3, "U32", "PF"   ),
            "cVP1"      : modbus_reg_t("Phase 1 centivolts"   , 0xc552, 3, "U32", "cVP1" ),
            "cVP2"      : modbus_reg_t("Phase 2 centivolts"   , 0xc554, 3, "U32", "cVP2" ),
            "cVP3"      : modbus_reg_t("Phase 3 centivolts"   , 0xc556, 3, "U32", "cVP3" ),
            "mAP1"      : modbus_reg_t("Phase 1 milliamps"    , 0xc560, 3, "U32", "mAP1" ),
            "mAP2"      : modbus_reg_t("Phase 2 milliamps"    , 0xc562, 3, "U32", "mAP2" ),
            "mAP3"      : modbus_reg_t("Phase 3 milliamps"    , 0xc564, 3, "U32", "mAP3" ),
            "ImEn"      : modbus_reg_t("Import Energy"        , 0xc652, 3, "U32", "ImEn" ),
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
        self._ll.write("adcs_cal")

    def fw_upload(self, filmware):
        from crccheck.crc import CrcModbus
        data = open(filmware, "rb").read()
        crc = CrcModbus.calc(data)
        hdata = ["%02x" % x for x in data]
        hdata = "".join(hdata)
        mtu=56
        for n in range(0, len(hdata), mtu):
            debug_print("Chunk %u/%u" % (n/2, len(hdata)/2))
            chunk=hdata[n:n + mtu]
        r = self.do_cmd("fw+ "+chunk)
            expect= "FW %u chunk added" % (len(chunk)/2)
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
    def __init__(self, port="/dev/ttyUSB0"):
        super().__init__(port)
        self._leftover = ""

    def parse_msg(self, msg):
        """
        IN:  'DEBUG:0000036805:DEBUG:TEMP:2113'
        OUT: ('TEMP', 2113)
        """
        r = re.search("DEBUG:[0-9]{10}:DEBUG:[A-Z0-9]+:FAILED", msg)
        if r and r.group(0):
            _,ts,_,name,_ = r.group(0).split(":")
            self._log(f"{name} failed.")
            return (name, False)
        r = re.search("DEBUG:[0-9]{10}:DEBUG:[A-Z0-9]+:[U8|U16|U32|U64|I8|I16|I32|I64|F]+:[0-9]+", msg)
        if r and r.group(0):
            _,ts,_,name,type_,value = r.group(0).split(":")
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
        return None
