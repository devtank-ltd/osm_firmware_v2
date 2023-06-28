import argparse
import binding
import time
import json

class json_to_dev:
    def __init__(self, dev):
        self.dev = binding.dev_t(dev)

    def verify_file(self, filename):
        with open(filename, 'r') as f:
            contents = f.read()
            parsed = json.loads(contents)
        if parsed:
            int_mins = parsed["interval_mins"]
            if int_mins:
                print("File verified, writing to osm.")
                self.load_json_to_osm(parsed)
            else:
                print("Invalid fields in file.")
                return False
        else:
            print("Invalid json.")
            return False

    def load_json_to_osm(self, contents):
        self.dev.do_cmd("wipe")
        time.sleep(2)
        if contents:
            self.dev.set_serial_num(contents["serial_num"])

            new_int_mins = contents["interval_mins"]
            self.dev.interval_mins = new_int_mins

            dev_eui = contents["dev_eui"]
            self.dev.dev_eui = dev_eui

            app_key = contents["app_key"]
            self.dev.app_key = app_key

            self.dev.update_midpoint("CC1", contents["cc_midpoints"]["CC1"])
            self.dev.update_midpoint("CC2", contents["cc_midpoints"]["CC2"])
            self.dev.update_midpoint("CC3", contents["cc_midpoints"]["CC3"])

            mb_setup = contents["modbus_bus"]["setup"]
            if mb_setup:
                protocol = mb_setup[0]
                baud = mb_setup[1]
                bit_par_stp = mb_setup[2]
                self.dev.do_cmd(f"mb_setup {protocol} {baud} {bit_par_stp}")

            mb_devices = contents["modbus_bus"]["modbus_devices"]
            if mb_devices:
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


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='JSON to Dev',
        description='Write config json file to OSM.',
        epilog='Run this program with the device and file as an argument e.g. python load_json_config.py /dev/ttyUSB0 /tmp/osm_sensor_01.json'
    )
    parser.add_argument('device')
    parser.add_argument('file')
    args = parser.parse_args()
    dev = json_to_dev(args.device)
    filepath = args.file
    dev.verify_file(filepath)
