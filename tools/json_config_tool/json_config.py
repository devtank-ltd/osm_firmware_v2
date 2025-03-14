#! /usr/bin/env python3

import os
import sys
import argparse
import json
import datetime
import re
import time

sys.path.append(os.path.join(os.path.dirname(__file__), "../../python/"))

import binding


def log(msg):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
    print("[%s] JSON : %s" % (now, msg), file=sys.stderr, flush=True)

IOS_PATTERN = "^IO (?P<io>[0-9]{2}) : +(\[(?P<specials_avail>[A-Za-z0-9 \|]+)\])? ((USED (?P<special_used>[A-Za-z0-9]+)( (?P<edge>F|R|B))?)|(?P<dir>IN|OUT)) (?P<pupd>DOWN|UP|NONE|D|U|N)( = (?P<level>ON|OFF))?$"

class dev_json_t:
    def __init__(self, dev):
        self.dev = dev
        assert isinstance(self.dev, binding.dev_t), log("Error: device not an instance of binding serial device.")

    def get_config(self):
        modbus = self.dev.get_modbus()
        self.name = self.dev.name
        self.mb_config = modbus.config
        self.modbus_devices = modbus.devices
        self.interval_mins = self.dev.interval_mins
        self.measurements = self.dev.measurements()
        self.meas_formatted = []
        for i in range(len(self.measurements)):
                row = self.measurements[i]
                for n in range(len(row)):
                    entry = row[n]
                    if type(entry) != int:
                        pos = entry.find("x")
                    if pos != -1:
                        row[n] = entry[0:pos]
                self.meas_formatted.append(row)

        self.ios = self.dev.do_cmd_multi("ios")
        self.fw = self.dev.version.value
        self.serial_num = self.dev.serial_num.value
        self.comms = self.dev.comms.as_dict()
        self.cc1_mp = self.dev.cc_midpoint("CC1")
        self.cc2_mp = self.dev.cc_midpoint("CC2")
        self.cc3_mp = self.dev.cc_midpoint("CC3")
        modbus_exists = self.modbus_devices
        self.modbus_devs = []
        if modbus_exists:
            for dev in modbus_exists:
                mb_regs = []
                tmp_list = []
                for reg in dev.regs:
                    i = (str(reg).replace(" ", "").split(','))
                    tmp_list.append(i)
                for i in range(len(tmp_list)):
                    hx = hex(int(tmp_list[i][1]))
                    row = list(tmp_list[i])
                    row[1] = hx
                    mb_regs.append(row)
                dict = {"name": dev.name,
                        "byteorder": dev.byteorder,
                        "wordorder": dev.wordorder,
                        "unit": dev.unit,
                        "registers": []}
                for i in mb_regs:
                    dict["registers"].append({
                        "reg"      : i[0],
                        "address"  : i[1],
                        "function" : i[2],
                        "type"     : i[3]
                        })
                self.modbus_devs.append(dict)

    def as_dict(self):
        json_pop = {
            "version": self.fw,
            "name": self.name,
            "serial_num": self.serial_num,
            "interval_mins":self.interval_mins,
            "comms": self.comms,
            "ios": {},
            "cc_midpoints": {
                "CC1":self.cc1_mp,
                "CC2":self.cc2_mp,
                "CC3":self.cc3_mp
            },
            "modbus_bus": {
                "setup": None,
                "modbus_devices": [],
            },
            "measurements":{}
        }

        json_ios = json_pop["ios"]

        for i, v in enumerate(self.ios):
            m = re.match(IOS_PATTERN, v)
            if not m:
                log("Invalid IOS configuration.")
                exit(1)
            re_dict = m.groupdict()
            spec = re_dict.get("special_used")
            if spec:
                json_ios.update({i:{"special": spec}})
            else:
                json_ios.update({i:{}})

            edge = re_dict.get("edge")
            if edge:
                json_ios[i].update({"edge": edge})

            pupd = re_dict.get("pupd")
            if pupd == "D":
                pupd = "DOWN"
            elif pupd == "U":
                pupd = "UP"
            elif pupd == "N":
                pupd = "NONE"
            if pupd:
                json_ios[i].update({"pull": pupd})

            direction = re_dict.get("dir")
            if direction:
                json_ios[i].update({"direction": direction})
            elif not direction and not spec:
                json_ios[i].update({"direction": "IN"})

        if self.mb_config:
            json_pop["modbus_bus"] = { "setup" : self.mb_config,
                                       "modbus_devices" : self.modbus_devs }
        for i in self.meas_formatted:
            json_pop["measurements"].update({
                i[0]:{
                    "interval": i[1],
                    "samplecount": i[2]
                    }
                })
        return json_pop

    def save_config(self, filepath:str):
        json_pop = self.as_dict()

        json_data = json.dumps(json_pop, indent=2)

        with open(filepath, 'w+') if filepath is not sys.stdout else sys.stdout as f:
            f.write(json_data)

        log(f"Config file saved in {filepath}")
        return filepath

    def load_config(self, filename):
        with open(filename, 'r') if filename is not sys.stdin else sys.stdin as f:
            contents = f.read()

        parsed = json.loads(contents)
        if parsed:
            int_mins = parsed["interval_mins"]
            if int_mins:
                log("File verified, writing to osm.")
                self.from_dict(parsed)
            else:
                log("Invalid fields in file.")
                return False
        else:
            log("Invalid json.")
            return False

    def from_dict(self, contents):
        self.dev.do_cmd("wipe")
        time.sleep(2)
        if contents:
            self.dev.set_serial_num(contents["serial_num"])
            self.dev.set_name(contents["name"])

            new_int_mins = contents["interval_mins"]
            self.dev.interval_mins = new_int_mins

            self.dev.comms.from_dict(contents["comms"])

            ios = contents["ios"]
            spec = edge = pull = None
            for i in ios:
                try:
                    spec = ios[i]["special"]
                except:
                    log(f"No special for IO 0{i}")
                if spec:
                    try:
                        edge = ios[i]["edge"]
                    except:
                        log(f"No edge for IO 0{i}")
                    try:
                        pull = ios[i]["pull"]
                    except:
                        log(f"No pull for IO 0{i}")
                    if spec == 'PLSCNT':
                        self.dev.do_cmd(f"en_pulse {i} {edge} {pull}")
                    elif spec == 'WATCH':
                        self.dev.do_cmd(f"en_watch {i} {pull}")
                    elif spec == 'W1':
                        self.dev.do_cmd(f"en_w1 {i} {pull}")
                else:
                    self.dev.disable_io(i)

            cc_midpoints = contents["cc_midpoints"]
            for cc, value in cc_midpoints.items():
                self.dev.set_cc_midpoint(value, cc)

            mb_contents = contents.get("modbus_bus")

            if mb_contents:
                mb_setup = mb_contents["setup"]
                if mb_setup:
                    protocol = mb_setup[0]
                    baud = mb_setup[1]
                    bit_par_stp = mb_setup[2]
                    self.dev.do_cmd(f"mb_setup {protocol} {baud} {bit_par_stp}")

                mb_devices = mb_contents["modbus_devices"]
                for dev in mb_devices:
                    name = dev["name"]
                    byteorder = dev["byteorder"]
                    wordorder = dev["wordorder"]
                    unit = dev["unit"]
                    self.dev.do_cmd(f"mb_dev_add {unit} {byteorder} {wordorder} {name}", timeout=3)
                    for reg in dev["registers"]:
                        hex = reg["address"]
                        function = reg["function"]
                        type = reg["type"]
                        name = reg["reg"]
                        self.dev.do_cmd(f"mb_reg_add {unit} {hex} {type} {function} {name}")

            measurements = contents["measurements"]
            for meas in measurements:
                interval = measurements[meas]["interval"]
                samplecount = measurements[meas]["samplecount"]
                self.dev.do_cmd(f"interval {meas} {interval}")
                self.dev.do_cmd(f"samplecount {meas} {samplecount}")
            self.dev.do_cmd("save")
            self.dev.drain()


def create_json_dev(dev):
    return dev_json_t(dev)


def main(args):
    dev = binding.dev_t(args.device)
    json_conv = create_json_dev(dev)
    ret = 0
    if sys.stdin.isatty():
        json_conv.get_config()
        json_conv.save_config(sys.stdout)
    else:
        ret = 0 if json_conv.load_config(sys.stdin) else -1
    return ret

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Save OSM configuration to a json file.',
        epilog='Run this program with the device as an argument e.g. python save_json_config.py /dev/ttyUSB0'
    )
    parser.add_argument('device')
    args = parser.parse_args()
    sys.exit(main(args))
