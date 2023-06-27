import argparse
import binding
import json
import re

class dev_to_json:
    def __init__(self, dev):
        self.dev = binding.dev_t(dev)
    
    def get_config(self):
        modbus = self.dev.get_modbus()
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
        self.dev_eui = self.dev.dev_eui
        self.app_key = self.dev.app_key
        self.cc1_mp = self.dev.get_midpoint("CC1")[0].split()[1]
        self.cc2_mp = self.dev.get_midpoint("CC2")[0].split()[1]
        self.cc3_mp = self.dev.get_midpoint("CC3")[0].split()[1]
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

    def save_config(self):
        serial = "unknown"
        if self.serial_num:
            s = self.serial_num.split("-")[-1]
            match = re.findall(r"0*([0-9]+)", s)
            serial = match[0]

        json_pop = {
            "version": None,
            "serial_num": None,
            "interval_mins":None,
            "dev_eui": None,
            "app_key": None,
            "cc_midpoints": {
                "CC1":None,
                "CC2":None,
                "CC3":None
            },
            "modbus_bus": {
                "setup": None,
                "modbus_devices": [],
            },
            "measurements":{}
        }
        json_pop["serial_num"] = self.serial_num
        json_pop["version"] = self.fw
        json_pop["interval_mins"] = self.interval_mins
        json_pop["dev_eui"] = self.dev_eui
        json_pop["app_key"] = self.app_key
        json_pop["cc_midpoints"]["CC1"] = self.cc1_mp
        json_pop["cc_midpoints"]["CC2"] = self.cc2_mp
        json_pop["cc_midpoints"]["CC3"] = self.cc3_mp
        if self.mb_config:
            json_pop["modbus_bus"]["setup"] = self.mb_config
            json_pop["modbus_bus"]["modbus_devices"] = self.modbus_devs
        for i in self.meas_formatted:
            json_pop["measurements"].update({
                i[0]:{
                    "interval": i[1],
                    "samplecount": i[2]
                    }
                })
        
        json_data = json.dumps(json_pop, indent=2)

        with open(f'/tmp/osm_sensor_{serial}.json', 'w+') as f:
            f.write(json_data)
        
        print(f"Config file saved in /tmp/osm_sensor_{serial}.json")
        return f"/tmp/osm_sensor_{serial}.json"

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='Dev to JSON',
        description='Save OSM configuration to a json file.',
        epilog='Run this program with the device as an argument e.g. python save_json_config.py /dev/ttyUSB0'
    )
    parser.add_argument('device')
    args = parser.parse_args()
    dev = dev_to_json(args.device)
    dev.get_config()
    dev.save_config()
