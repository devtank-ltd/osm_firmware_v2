#! /usr/bin/python3


import serial
import datetime
import time
import csv
import sys
import io
import os
import re


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
        self._value = self.parse_func(value_str)
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
        if r:
            return r[-1]
        return False

    @property
    def cmd(self)->str:
        return f"mb_get_reg {self.handle}"


class log_t(object):
    def __init__(self, sender, receiver, log_file=None):
        self.log_file = log_file
        if log_file == None:
            self.log_file = sys.stdout
        self.sender = sender
        self.receiver = receiver

    def emit(self, payload):
        if self.log_file != None:
            now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
            print("[%s] %s\r"% (now, payload), file=self.log_file)

    def send(self, msg):
        self.emit("%s >>> %s : %s"% (self.sender, self.receiver, msg))

    def recv(self, msg):
        self.emit("%s <<< %s : %s"% (self.sender, self.receiver, msg))


class low_level_dev_t(object):
    def __init__(self, serial_obj, log_obj):
        self.log = log_obj
        self.serial = io.TextIOWrapper(io.BufferedRWPair(serial_obj, serial_obj), newline="\r\n")

    def write(self, msg):
        self.log.send(msg)
        self.serial.write("%s\n"% msg)
        self.serial.flush()
        time.sleep(0.1)

    def read(self):
        try:
            msg = self.serial.readline().strip("\r\n")
        except UnicodeDecodeError:
            return None
        if msg == '':
            return None
        self.log.recv(msg)
        return msg

    def readlines(self):
        msg = self.read()
        msgs = []
        while msg != None:
            msgs.append(msg)
            msg = self.read()
        return msgs


class dev_t(object):
    def __init__(self, port):
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

    def __enter__(self):
        if not self._serial_obj.is_open:
            p = self._serial_obj.open()
        self._dev = self.dev_unsafe_t(self._serial_obj)
        return self._dev

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self._serial_obj.close()

    class dev_unsafe_t(object):
        def __init__(self, serial_obj):
            self._log_obj = log_t("PC", "OSM")
            self._log = self._log_obj.emit
            self._ll = low_level_dev_t(serial_obj, self._log_obj)

        def do_cmd(self, cmd:str, timeout:float=1.5)->str:
            self._ll.write(cmd)
            end_time = time.monotonic() + timeout
            r = []
            while time.monotonic() < end_time:
                r += self._ll.readlines()
                if "ERROR" in r:
                    break
            r_str = ""
            for i in range(0, len(r)):
                line = r[i]
                r_str += str(line)
            return r_str

        def get_vals(self, cmds:list):
            debug_before = self._set_debug(0x1)
            for cmd in cmds:
                assert isinstance(cmd, measurement_t), "Commands should be of type measurement_t"
                cmd.value = self.do_cmd(cmd.cmd)
                if cmd.value is False:
                    time.sleep(4)
                    cmd.value = self.do_cmd(cmd.cmd)
            self._set_debug(debug_before)

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
            self._ll.write("mb_setup BIN 9600 8N1")
            return True

        def modbus_dev_del(self, device:str):
            self._ll.write(f"mb_dev_del {device}")

        def get_modbus(self, slave_id:int, device:str, regs:list)->bool:
            if not self.setup_modbus_dev(slave_id, device, regs):
                return False
            for reading in regs:
                reading.value = self.do_cmd(reading.cmd)
                if reading.value is False:
                    reading.value = self.do_cmd(reading.cmd)
            self.modbus_dev_del(device)
            return True

        def current_clamp_calibrate(self):
            self._ll.write("adcs_cal")


class csv_obj_t(object):
    def __init__(self, csv_loc):
        self._csv_loc = csv_loc

    def __enter__(self):
        self._csv_file = open(self._csv_loc, "w")
        return self.csv_obj_unsafe_t(self._csv_file)

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self._csv_file.close()

    class csv_obj_unsafe_t(object):
        def __init__(self, csv_file):
            self._csv_file = csv_file

        def write_data(self, commands:list)->None:
            date_time = datetime.datetime.now()
            self._csv_file.write(f"Time,{date_time:%H:%M:%S\n,%d/%m/%Y}\n")
            assert isinstance(commands, list), "Data should be a dictionary"
            for cmd in commands:
                assert isinstance(cmd, measurement_t), "Commands not of type measurement_t"
                val_str = cmd.value
                if isinstance(cmd.value, list):
                    val_str = f"{cmd.value[0]}"
                    for i in range(1, len(cmd.value)):
                        val_str += f"|{cmd.value[i]}"
                self._csv_file.write(f"{cmd.name},{val_str}\n")


def parse_one_wire(r_str:str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return r[-1]
    return False


def parse_current_clamp(r_str:str):
    if "ERROR" in r_str:
        return False
    r = re.findall(r"[-+]?(?:\d*\.\d+|\d+)", r_str)
    if r:
        return r[-1]
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
        return [r[-3],r[-1]]
    return False


def parse_lora_comms(r_str:str):
    return "Connected" in r_str


commands = [ measurement_t("One Wire"           , float , "w1"        , parse_one_wire       ) ,
             measurement_t("Current Clamp"      , int   , "adcs"      , parse_current_clamp  ) ,
             measurement_t("Particles (2.5|10)" , str   , "hpm 1"     , parse_particles      ) ,
             measurement_t("LoRa Comms"         , bool  , "lora_conn" , parse_lora_comms     ) ]


mb_regs = [ modbus_reg_t("Power Factor"         , 0xc56e, 3, "U32", "PF"   ) ,
            modbus_reg_t("Phase 1 centivolts"   , 0xc552, 3, "U32", "cVP1" ) ,
            modbus_reg_t("Phase 2 centivolts"   , 0xc554, 3, "U32", "cVP2" ) ,
            modbus_reg_t("Phase 3 centivolts"   , 0xc556, 3, "U32", "cVP3" ) ,
            modbus_reg_t("Phase 1 milliamps"    , 0xc560, 3, "U32", "mAP1" ) ,
            modbus_reg_t("Phase 2 milliamps"    , 0xc562, 3, "U32", "mAP2" ) ,
            modbus_reg_t("Phase 3 milliamps"    , 0xc564, 3, "U32", "mAP3" ) ,
            modbus_reg_t("Import Energy"        , 0xc652, 3, "U32", "ImEn" ) ]


def print_usage():
    print("Usage:")
    print(f"./{sys.argv[0]} [port] [filename]")


def parse_args():
    port = "/dev/ttyUSB0"
    file_name = "results.csv"
    argc = len(sys.argv)
    print(sys.argv)
    if argc > 3:
        print_usage()
        exit()
    elif argc == 3:
        _, port, file_name = sys.argv
    elif argc == 2:
        _, port = sys.argv
    elif argc == 1:
        pass
    else:
        print_usage()
        exit()
    return (port, file_name)


def main():
    port, file_name = parse_args()
    dev = dev_t(port)
    with dev as d:
        input("Ensure current clamp (ADC in) is unplugged/off. Press Enter to continue.")
        lock_until = time.monotonic() + 2
        d.current_clamp_calibrate()
        while (time.monotonic() < lock_until):
            pass
        input("Plug current clamp (ADC in) in. Press Enter to continue.")
        d.get_vals(commands)
        d.get_modbus(5, "E53", mb_regs)
    csv_obj = csv_obj_t(file_name)
    with csv_obj as c:
        c.write_data(commands + mb_regs)
    return 0


if __name__ == "__main__":
    start = time.monotonic()
    main()
    print(f"Took {time.monotonic() - start}s to complete.");
