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

def app_key_generator(size=32, chars=string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))

def dev_eui_generator(size=16, chars=string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))

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
        self._serial = io.TextIOWrapper(io.BufferedRWPair(serial_obj, serial_obj), newline="\n")
        self._log_obj = log_obj
        self.fileno = serial_obj.fileno

    def write(self, msg):
        self._log_obj.send(msg)
        self._serial.write("%s\n" % msg)
        self._serial.flush()
        time.sleep(0.05)

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
        self._log('No attribute "%s"' % attr)
        raise AttributeError

    def _log(self, msg):
        self._log_obj.emit(msg)
 
    @property
    def interval_mins(self):
        return self.do_cmd("interval_mins")

    def get_modbus_val(self, val_name, timeout:float=1.5):
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
        
        
    def extract_val(self, val_name, timeout:float=1.5):
        self._ll.write(val_name)
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
                        print(regex)
                        new_r = regex[0]
                        new_str = new_r[:-1]
                        return int(new_str)
                    except ValueError:
                        pass
                elif ("Lux: ") in line:
                    try:
                        return int(line.split(':')[-1])
                    except ValueError:
                        pass
                elif ("PM25:") in line:
                    try:
                        pm_values = re.split('[:,]', line)
                        return pm_values[1], pm_values[3]
                    except ValueError:
                        pass
        return False
            
    def extract_pm25(self, val_name, timeout:float=1.5):
        self._ll.write(val_name)
        end_time = time.monotonic() + timeout
        last_line = ""
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            if not new_lines:
                continue
            new_lines[0] = last_line + new_lines[0]
            last_line = new_lines[-1]  
            for line in new_lines:
                if ("PM25:") in line:
                    try:
                        self.pm_values = re.split('[:,]', line)
                        return int(self.pm_values[1])
                    except ValueError:
                        pass
        return False
    
    def extract_pm10(self, val_name, timeout:float=1.5):
        self._ll.write(val_name)
        end_time = time.monotonic() + timeout
        last_line = ""
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            if not new_lines:
                continue
            new_lines[0] = last_line + new_lines[0]
            last_line = new_lines[-1]  
            for line in new_lines:
                if ("PM25:") in line:
                    try:
                        pm_values = re.split('[:,]', line)
                        return int(pm_values[3])
                    except ValueError:
                        pass
        return False
        
        
        
    def do_cmd(self, cmd:str, timeout:float=1.5)->str:
        self._ll.write(cmd)
        end_time = time.monotonic() + timeout
        r = []
        done = False
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            for line in new_lines:
                #self.terminal.run_command('echo %s \n' % line)
                if "}============" in line:
                    done = True
            r += new_lines
            if done:
                break
            assert not "ERROR" in r, "OSM Error"
        r_str = ""
        for i in range(0, len(r)):
            line = r[i]
            r_str += str(line)
        return r_str

    def do_cmd_multi(self, cmd:str, timeout:float=1.5)->str:
        self._ll.write(cmd)
        end_time = time.monotonic() + timeout
        r = []
        done = False
        while time.monotonic() < end_time:
            new_lines = self._ll.readlines()
            for line in new_lines:
                self.terminal.run_command('echo %s \n' % line)
                if "}============" in line:
                    done = True
            r += new_lines
            if done:
                break
        start_pos = None
        end_pos   = None
        for n in range(0, len(r)):
            line = r[n]
            if line == "============{":
                start_pos = n+1
            elif line == "}============":
                end_pos = n-1
        if start_pos is not None and end_pos is not None:
            return r[start_pos:end_pos]
        return None
    
    def terminal_gen(self):
        self.terminal = Terminal(pady=5, padx=5, background='black', foreground='white')
        self.terminal.grid(column=1, row=13)
        self.terminal.linebar = True
        self.terminal.shell = True
        self.terminal.basename = "OSMdev"

    def save_json_img(self):
        os.system("sudo ./tools/config_scripts/config_save.sh /tmp/my_osm_config.img")
        os.system("./tools/build/json_x_img /tmp/my_osm_config.img > /tmp/my_osm_config.json")

    def lora_debug(self):
        self.do_cmd("debug 4")
    
    def see_meas(self):
        self.do_cmd("measurements")

    def wipe_clean(self):
        self.do_cmd("wipe")

    def debug_mb(self):
        self.do_cmd("debug 0x200")
    
    def mb_log(self):
        self.do_cmd("mb_log")

    def add_rif_dev(self):
        self.do_cmd("mb_dev_add 1 RIF")
    
    def mb_setup(self):
        self.do_cmd("mb_setup RTU 9600 8N1")
    
    def add_e53_dev(self):
        self.do_cmd("mb_dev_add 5 E53")

    def measurements(self):
        r = self.do_cmd_multi("measurements")
        r = r[2:]
        return [ line.replace("\t\t","\t").split("\t") for line in r]

    def add_e53_reg(self):
        regs = ["5 0xC56E 3 U32 PF",
                "5 0xC552 3 U32 cVP1",
                "5 0xC554 3 U32 cVP2",
                "5 0xC556 3 U32 cVP3",
                "5 0xC560 3 U32 mAP1",
                "5 0xC562 3 U32 mAP2",
                "5 0xC564 3 U32 mAP3",
                "5 0xC652 3 U32 ImEn"]
        for reg in regs:
            self.do_cmd(f"mb_reg_add {reg}")

    def add_modbus_reg(self):
        regs = ["1 0x36 4 F PF",
                "1 0x00 4 F VP1",
                "1 0x02 4 F VP2",
                "1 0x04 4 F VP3",
                "1 0x10 4 F AP1",
                "1 0x12 4 F AP2",
                "1 0x14 4 F AP3",
                "1 0x60 4 F Imp"]
        for reg in regs:
            self.do_cmd(f"mb_reg_add {reg}")

    def get_value(self, cmd):
        cmd = self.do_cmd(cmd)
        if cmd is False:
            time.sleep(4)
            cmd = self.do_cmd(cmd)

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

    def modbus_reg_add(self, slave_id:int, reg:modbus_reg_t)->bool:
        if not isinstance(reg, modbus_reg_t):
            self._log("Registers should be an object of register")
            return False
        r = self.do_cmd(f"mb_reg_add {slave_id} {hex(reg.address)} {reg.func} {reg.mb_type_} {reg.handle}")
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

    def write_lora(self):
        app_key = app_key_generator()
        self.do_cmd("lora_config app-key %s" % app_key)

    def write_eui(self):
        dev_eui = dev_eui_generator()
        self.do_cmd("lora_config dev-eui %s" % dev_eui)


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
        r = re.search("DEBUG:[0-9]{10}:DEBUG:[A-Za-z0-9]+:FAILED", msg)
        if r and r.group(0):
            _,ts,_,name,_ = r.group(0).split(":")
            self._log(f"{name} failed.")
            return (name, False)
        r = re.search("DEBUG:[0-9]{10}:DEBUG:[A-Za-z0-9]+:[U8|U16|U32|U64|I8|I16|I32|I64|F]+:[0-9]+", msg)
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
