#!/usr/bin/env python3

import os
import re
import sys
import math
import time
import errno
import subprocess
import signal
import multiprocessing
import serial
import select
import yaml
import json
import ssl
import paho.mqtt as paho
import paho.mqtt.client as mqtt
from binding import modbus_reg_t, dev_t, set_debug_print, lw_comms_t, wifi_comms_t

sys.path.append("../ports/linux/peripherals/")

import comms_connection as comms

import basetypes

import modbus_db

sys.path.append("../tools/json_config_tool/")

import json_config

import wifi_fw_download


class test_framework_t(object):

    DEFAULT_OSM_BASE        = "/tmp/osm/"
    DEFAULT_OSM_CONFIG      = DEFAULT_OSM_BASE + "osm.img"
    DEFAULT_DEBUG_PTY_PATH  = DEFAULT_OSM_BASE + "UART_DEBUG_slave"
    DEFAULT_COMMS_PTY_PATH  = DEFAULT_OSM_BASE + "UART_COMMS_slave"
    DEFAULT_VALGRIND        = "valgrind"
    DEFAULT_VALGRIND_FLAGS  = "--leak-check=full"
    DEFAULT_PROTOCOL_PATH   = "%s/../lorawan_protocol/debug.js"% os.path.dirname(__file__)

    JSON_MEMORY_PATH        = DEFAULT_OSM_BASE + "osm.json"

    INTERVAL_MINS           = 0.083

    DEFAULT_COMMS_MATCH_DICT = {
          "PM10"    : 30,
          "PM10_min": 30,
          "PM10_max": 30,
          "PM25"    : 20,
          "PM25_min": 20,
          "PM25_max": 20,
          "TMP2"    : 25.087,
          "TMP2_min": 25.087,
          "TMP2_max": 25.087,
          "TEMP"    : 21.59,
          "TEMP_min": 21.59,
          "TEMP_max": 21.59,
          "HUMI"    : 65.284,
          "HUMI_min": 65.284,
          "HUMI_max": 65.284,
          "PF"      : 1023,
        }

    def __init__(self, osm_path, log_file=None, mqtt_info=None, do_ota=False):
        self._logger = basetypes.get_logger(log_file)
        self._log_file = log_file

        self.do_ota = do_ota

        mqtt_sch = 1
        if mqtt_info.get("use_ssl", True):
            mqtt_sch += 2
            if mqtt_info.get("insecure", True):
                mqtt_sch -= 1
        if mqtt_info.get("websockets", True):
            mqtt_sch += 5
        self._logger.debug(f"USING SCHEME: {mqtt_sch}")

        self._wifi_conf = {
            "wifi_ssid" : "none",
            "wifi_pwd"  : "none",
            "mqtt_addr" : mqtt_info.get("address", "localhost"),
            "mqtt_user" : mqtt_info.get("user", "mqtt_user"),
            "mqtt_pwd"  : mqtt_info.get("password", "mqtt_password"),
            "mqtt_sch"  : mqtt_info.get("scheme", mqtt_sch),
            "mqtt_port" : mqtt_info.get("port", 9001),
            "mqtt_ca"   : mqtt_info.get("ca", "none"),
        }

        if not os.path.exists(osm_path):
            self._logger.error("Cannot find fake OSM.")
            raise AttributeError("Cannot find fake OSM.")

        self._vosm_path         = osm_path

        self._vosm_proc         = None
        self._vosm_conn         = None

        self._done              = False

        self._db                = modbus_db.modb_database_t(modbus_db.find_path())

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.exit()

    def exit(self):
        self._disconnect_osm()
        self._close_virtual_osm()

    def _wait_for_file(self, path, timeout):
        start_time = time.monotonic()
        while not os.path.exists(path):
            if start_time + timeout <= time.monotonic():
                self._logger.error(f'The path "{path}" does not exist.')
                return False
        return True

    def _threshold_check(self, name, desc, value, ref, tolerance):
        if isinstance(value, str):
            assert name == "FW"
            ref = '"%s"' % os.popen('printf "%.*s\n" 7 $(git log -n 1 --format="%H")').read().strip()
            passed = value == ref
            tolerance = 0
        elif isinstance(value, float) or isinstance(value, int):
            passed = abs(float(value) - float(ref)) <= float(tolerance)
        else:
            self._logger.debug(f'Invalid test argument {value} for "{desc}"')
            passed = False
        op = "=" if passed else "!="
        prefix = poxtfix = ""
        if self._log_file is None:
            prefix = basetypes.test_logging_formatter_t.GREEN if passed else basetypes.test_logging_formatter_t.RED
            poxtfix = basetypes.test_logging_formatter_t.RESET
        self._logger.info(prefix + f'{desc} = {"PASSED" if passed else "FAILED"} ({value} {op} {ref} +/- {tolerance})' + poxtfix)
        return passed

    def _bool_check(self, desc, value, ref:bool):
        if isinstance(value, bool):
            passed = value == ref
        else:
            self._logger.debug(f'Invalid test argument {value} for "{desc}"')
            passed = False
        op = "=" if passed else "!="
        prefix = poxtfix = ""
        if self._log_file is None:
            prefix = basetypes.test_logging_formatter_t.GREEN if passed else basetypes.test_logging_formatter_t.RED
            poxtfix = basetypes.test_logging_formatter_t.RESET
        self._logger.info(prefix + f'{desc} = {"PASSED" if passed else "FAILED"} ({value} {op} {ref})' + poxtfix)
        return passed

    def _str_check(self, desc, value, ref:str):
        if not isinstance(value, str):
            self._logger.debug(f'Invalid test argument {value} for "{desc}"')
            passed = False
        else:
            passed = value == ref
        op = "=" if passed else "!="
        prefix = poxtfix = ""
        if self._log_file is None:
            prefix = basetypes.test_logging_formatter_t.GREEN if passed else basetypes.test_logging_formatter_t.RED
            poxtfix = basetypes.test_logging_formatter_t.RESET
        self._logger.info(prefix + f'{desc} = {"PASSED" if passed else "FAILED"} ({value} {op} {ref})' + poxtfix)
        return passed

    def _start_osm_env(self):
        if not self._spawn_virtual_osm(self._vosm_path):
            self._logger.error("Failed to spawn virtual OSM.")
            return False
        return True

    def _get_active_measurements(self) -> list:
        actives = []
        for measurement in self._vosm_conn.measurements():
            if len(measurement) != 3:
                continue
            name, interval_txt, sample_count_txt = measurement
            interval = int(interval_txt.split("x")[0])
            sample_count = int(sample_count_txt)
            if interval and sample_count:
                actives.append(name)
        return actives

    def _get_measurement_info(self, measurement_handle):
        DEFAULT_MODBUS_MATCH_DICT = {
          "PF"      : 1000,
          "VP1"     : 240,
          "VP2"     : 240,
          "VP3"     : 240,
          "AP1"     : 30,
          "AP2"     : 30,
          "AP3"     : 30,
          "cVP1"    : 24000,
          "cVP2"    : 24000,
          "cVP3"    : 24000,
          "mAP1"    : 3000,
          "mAP2"    : 3000,
          "mAP3"    : 3000
        }
        self._logger.debug(f"DB RETRIEVE MEASUREMENT '{measurement_handle}'...")
        measurement_obj = getattr(self._vosm_conn, measurement_handle)
        resp = self._db.get_measurement_info(measurement_handle)
        if resp is None:
            self._logger.debug(f"DB '{measurement_handle}' IS NOT REGULAR, CHECK MODBUS...")
            resp = self._db.get_modbus_measurement_info(measurement_handle)
            if resp is None:
                self._logger.debug(f"DB NO DATA FOR '{measurement_handle}'")
                return (measurement_handle, measurement_obj, 0, 0)
            else:
                description = resp[0]
                ref = DEFAULT_MODBUS_MATCH_DICT[measurement_handle]
                #Get threshold by calculating 5% of reference
                threshold = ref*5 / 100
        else:
            description, ref, threshold = resp
        data = (description, measurement_obj, ref, threshold)
        self._logger.debug(f"DB GOT MEASUREMENT '{measurement_handle}: '{data}'")
        return data

    def _check_interval_mins_val(self):
        im = self._vosm_conn.interval_mins
        return self._bool_check("Interval minutes is a valid float and is reading correct value.", float(im) > 0 and float(im) == self.INTERVAL_MINS, True)

    def _check_lora_config_val(self):
        appk = self._vosm_conn.comms.app_key
        deveui = self._vosm_conn.comms.dev_eui
        return  (self._bool_check("Application key is a valid type.",
                                  isinstance(appk, str) and appk=="LINUX-APP", True) and
                 self._bool_check("Device EUI is a valid type.",
                                  isinstance(deveui, str) and deveui=="LINUX-DEV", True))

    def _check_wifi_config_val(self):
        self._wifi_set_conf()
        ret = True
        for name, conf, ref in [
            ("WiFi SSID"        , self._vosm_conn.comms.wifi_ssid   , self._wifi_conf["wifi_ssid"]  ),
            ("WiFi Password"    , self._vosm_conn.comms.wifi_pwd    , self._wifi_conf["wifi_pwd"]   ),
            ("MQTT Address"     , self._vosm_conn.comms.mqtt_addr   , self._wifi_conf["mqtt_addr"]  ),
            ("MQTT User"        , self._vosm_conn.comms.mqtt_user   , self._wifi_conf["mqtt_user"]  ),
            ("MQTT Password"    , self._vosm_conn.comms.mqtt_pwd    , self._wifi_conf["mqtt_pwd"]   ),
            ("MQTT CA"          , self._vosm_conn.comms.mqtt_ca     , self._wifi_conf["mqtt_ca"]    ),
            ]:
            ret &= self._str_check(f"{name}", conf, ref)
        for name, conf, ref in [
            ("MQTT Scheme"      , self._vosm_conn.comms.mqtt_sch    , self._wifi_conf["mqtt_sch"]   ),
            ("MQTT Port"        , self._vosm_conn.comms.mqtt_port   , self._wifi_conf["mqtt_port"]  ),
            ]:
            try:
                conf_int = int(conf)
            except ValueError:
                ret &= self._bool_check(f"{name} cannot be integer", isinstance(conf, int), True)
            else:
                ret &= self._threshold_check(name, f"{name} setting", conf_int, int(ref), 0.)
        return ret

    def _check_cc_val(self):
        passed = True
        types_ = self._vosm_conn.cc_type()
        cc_g = self._vosm_conn.print_cc_gain
        for p in range(3):
            cc_name = "CC%u" % (p+1)
            unit = types_[p][-1]
            passed &= self._str_check(f"Valid {cc_name} Exterior value", cc_g[p*2], f"{cc_name} EXT max: 100.000A")
            passed &= self._str_check(f"Valid {cc_name} Interior value", cc_g[p*2+1], f"{cc_name} INT max: 0.050{unit}")
            mp = self._vosm_conn.get_midpoint(cc_name)
            passed &= self._str_check(f"{cc_name} Midpoint value is valid", mp[0], "MP: 2048.000")
        return passed

    def test(self):
        self._logger.info("Starting Virtual OSM Test...")

        if os.path.exists(self.DEFAULT_DEBUG_PTY_PATH):
            os.unlink(self.DEFAULT_DEBUG_PTY_PATH)

        os.environ["AUTO_MEAS"] = "0"
        os.environ["MEAS_INTERVAL"] = "1"

        if not self._start_osm_env():
            return False
        if not self._connect_osm(self.DEFAULT_DEBUG_PTY_PATH):
            return False
        passed = True
        for p in range(1,4):
            self._vosm_conn.update_midpoint(2048, f"CC{p}")
            self._vosm_conn.set_input_output_cc(p, 100, 50)
        self._vosm_conn.interval_mins = self.INTERVAL_MINS
        passed &= self._check_interval_mins_val()
        if isinstance(self._vosm_conn.comms, lw_comms_t):
            # If LW comms, check LW config
            passed &= self._check_lora_config_val()
        elif isinstance(self._vosm_conn.comms, wifi_comms_t):
            # If WiFi, check wifi config
            passed &= self._check_wifi_config_val()
        else:
            self._logger.error("Unknown comms config")
            passed = False
        passed &= self._check_cc_val()
        self._vosm_conn.measurements_enable(False)
        self._vosm_conn.PM10.interval = 1
        self._vosm_conn.PM25.interval = 1
        self._vosm_conn.TMP2.interval = 1
        self._vosm_conn.CC1.interval =  1
        self._vosm_conn.CC2.interval =  1
        self._vosm_conn.CC3.interval =  1
        self._vosm_conn.FTA1.interval = 1
        self._vosm_conn.FTA2.interval = 1
        self._vosm_conn.FTA3.interval = 1
        self._vosm_conn.FTA4.interval = 1
        self._vosm_conn.CC1.samplecount =  5
        self._vosm_conn.CC2.samplecount =  5
        self._vosm_conn.CC3.samplecount =  5
        self._vosm_conn.FTA1.samplecount = 5
        self._vosm_conn.FTA2.samplecount = 5
        self._vosm_conn.FTA3.samplecount = 5
        self._vosm_conn.FTA4.samplecount = 5

        self._vosm_conn.setup_modbus(is_bin=True)
        self._vosm_conn.setup_modbus_dev(5, "E53", True, True, [
            modbus_reg_t(self._vosm_conn, "PF"     , 0xc56e, 3, "U32"),
            modbus_reg_t(self._vosm_conn, "cVP1"   , 0xc552, 3, "U32")
            ])
        self._vosm_conn.setup_modbus_dev(1, "RIF", True, False, [
            modbus_reg_t(self._vosm_conn, "AP1" , 0x10, 4, "F" ),
            modbus_reg_t(self._vosm_conn, "AP2" , 0x12, 4, "F" )
            ])
        self._vosm_conn.get_modbus()

        for active in self._get_active_measurements():
            if active in ["SND", "NOX"]:
                continue
            description, measurement_obj, ref, threshold = self._get_measurement_info(active)
            passed &= self._threshold_check(active, description, getattr(measurement_obj, "value"), ref,  threshold)

        io = self._vosm_conn.ios[0]
        passed &= self._bool_check("IO off", io.value, False)
        io.value = True
        passed &= self._bool_check("IO still off", io.value, False)
        io.configure(False, "U")
        io.value = True
        passed &= self._bool_check("IO on", io.value, True)
        passed &= self._check_set_reg()

        # If the CCs are left on, measurement loop fails. TODO: Fix that
        self._vosm_conn.CC1.interval = 0
        self._vosm_conn.CC2.interval = 0
        self._vosm_conn.CC3.interval = 0

        self._logger.info("Running measurement loop...")
        self._vosm_conn.interval_mins = self.INTERVAL_MINS
        for sample in self.DEFAULT_COMMS_MATCH_DICT.keys():
            if not sample.endswith("_min") and not sample.endswith("_max"):
                self._vosm_conn.change_interval(sample, 1)
        self._vosm_conn.measurements_enable(True)

        if isinstance(self._vosm_conn.comms, lw_comms_t):
            passed &= self._check_lw_serial_comms()
        elif isinstance(self._vosm_conn.comms, wifi_comms_t):
            if self.do_ota:
                passed &= self._check_wifi_serial_comms()
        else:
            self._logger.error("Unknown comms config")
            passed = False

        passed &= self._check_json_config_tool()
        self._vosm_conn.measurements_enable(False)
        if self.do_ota:
            if isinstance(self._vosm_conn.comms, lw_comms_t):
                # TODO: If LW comms, LW OTA update
                pass
            elif isinstance(self._vosm_conn.comms, wifi_comms_t):
                passed &= self._check_wifi_ota_update()
            else:
                self._logger.error("Unknown comms config")
                passed = False
        return passed

    def _check_set_reg(self):
        mb_data = self._vosm_conn.get_modbus()
        if not len(mb_data.devices):
            self._logger.error("No MODBUS devices")
            return False
        self._logger.info("Running modbus set test...")
        for mb_dev in mb_data.devices:
            for reg in mb_dev.regs:
                # As setting registers only work for holding registers, dont bother for input registers.
                if reg.func != 3:
                    continue
                original_value = getattr(self._vosm_conn, reg.name).value
                new_set_value = original_value + 1
                if not self._vosm_conn.modbus_reg_set(reg=reg.name, value=new_set_value):
                    self._logger.error(f"Failed to set the register for {reg.name} = {new_set_value}")
                    return False
                new_value = getattr(self._vosm_conn, reg.name).value
                if new_set_value != new_value:
                    self._logger.debug(f"Failed to set {reg.name}. ({new_set_value} != {new_value})")
                    return False
                self._logger.debug(f"Successfully set {reg.name} from {original_value} to {new_set_value}")
                new_set_value2 = new_set_value + 1
                if not self._vosm_conn.modbus_reg_set(dev=mb_dev.name, reg_addr=reg.address, type=reg.mb_type_, value=new_set_value2):
                    self._logger.error(f"Failed to set the register for {hex(reg.address)} = {new_set_value2}")
                    return False
                new_value2 = getattr(self._vosm_conn, reg.name).value
                if new_set_value2 != new_value2:
                    self._logger.debug(f"Failed to set {mb_dev.name}:{hex(reg.address)} {reg.mb_type_}. ({new_set_value2} != {new_value2})")
                    return False
                self._logger.debug(f"Successfully set {mb_dev.name}:{hex(reg.address)} {reg.mb_type_} from {new_set_value} to {new_value2}")
                self._bool_check(f"Modbus register {reg.name} set test", True, True)

        return True

    def _check_json_config_tool(self):
        json_obj = json_config.create_json_dev(self._vosm_conn)
        json_obj.get_config()
        json_obj.save_config(self.JSON_MEMORY_PATH)

        with open(self.JSON_MEMORY_PATH, "r") as json_file:
            mem = json.load(json_file)

        interval_mins = 3.
        sc = 3
        mem["interval_mins"] = interval_mins
        mem["measurements"]["PM10"]["samplecount"] = sc

        with open(self.JSON_MEMORY_PATH, "w") as json_file:
            json.dump(mem, json_file)

        json_obj.load_config(self.JSON_MEMORY_PATH)

        with open(self.JSON_MEMORY_PATH, "r") as json_file:
            mem = json.load(json_file)

        new_mins = mem["interval_mins"]
        if float(new_mins) != interval_mins:
            self._logger.error(f"JSON failed to set interval_mins to {new_mins}.")
            return False
        self._logger.debug(f"Successfully set interval mins from 1 to {new_mins}")
        self._bool_check(f"JSON interval_mins set test", True, True)

        new_sc = mem["measurements"]["PM10"]["samplecount"]
        if new_sc != sc:
            self._logger.error(f"JSON failed to set PM10 sample count to {new_sc}.")
            return False
        self._logger.debug(f"Successfully set PM10 sample count from 5 to {new_sc}")
        self._bool_check(f"JSON sample count set test", True, True)
        return True

    def _wifi_set_conf(self):
        self._vosm_conn.comms.wifi_ssid     = self._wifi_conf["wifi_ssid"]
        self._vosm_conn.comms.wifi_pwd      = self._wifi_conf["wifi_pwd"]
        self._vosm_conn.comms.mqtt_addr     = self._wifi_conf["mqtt_addr"]
        self._vosm_conn.comms.mqtt_user     = self._wifi_conf["mqtt_user"]
        self._vosm_conn.comms.mqtt_pwd      = self._wifi_conf["mqtt_pwd"]
        self._vosm_conn.comms.mqtt_sch      = self._wifi_conf["mqtt_sch"]
        self._vosm_conn.comms.mqtt_port     = self._wifi_conf["mqtt_port"]
        self._vosm_conn.comms.mqtt_ca       = self._wifi_conf["mqtt_ca"]

    def _check_wifi_ota_update(self):
        self._wifi_set_conf()
        timeout = time.monotonic() + 25.
        connected = self._vosm_conn.comms_conn.value
        while not connected:
            time.sleep(0.5)
            if time.monotonic() > timeout:
                self._logger.error("Timed out waiting for MQTT connection")
                return False
            connected = self._vosm_conn.comms_conn.value

        firmware_filename = self._vosm_path
        firmware_filename_comp = f"{firmware_filename}.xz"
        if not os.path.exists(firmware_filename):
            self._logger.error(f"Firmware path doesnt exist: {firmware_filename}")
            return False
        if os.path.exists(firmware_filename_comp):
            os.unlink(firmware_filename_comp)
        subprocess.check_output(f"xz -k -9 {firmware_filename}", shell=True)
        firmware = open(firmware_filename_comp, "rb")
        use_websockets = self._wifi_conf["mqtt_sch"] > 5
        use_ssl = self._wifi_conf["mqtt_sch"] % 5 > 1
        insecure = self._wifi_conf["mqtt_sch"] % 5 < 3
        with wifi_fw_download.wifi_fw_dl_t(
            address=self._wifi_conf["mqtt_addr"],
            port=self._wifi_conf["mqtt_port"],
            firmware_file=firmware,
            hwid="00C0FFEE",
            user=self._wifi_conf["mqtt_user"],
            password=self._wifi_conf["mqtt_pwd"],
            ca=self._wifi_conf["mqtt_ca"],
            websockets=use_websockets,
            use_ssl=use_ssl,
            insecure=insecure,
            verbose="DEBUG" in os.environ,
            logger=self._logger) as wifi_fw_dl:
            wifi_fw_dl.run(
                loop_cb=self._vosm_conn.drain,
                args=(0.,)
                )
        r = self._vosm_conn._ll.readlines(end_line="----start----", timeout=10)
        self._bool_check("Rebooted", bool(self._vosm_conn.version.value), True)
        new_fw_path = os.path.join(self.DEFAULT_OSM_BASE, "new_firmware.elf")
        diff = subprocess.run(f"diff {firmware_filename} {new_fw_path}", shell=True)
        self._bool_check("OTA Update", not bool(diff.returncode), True)
        return True

    def _check_lw_serial_comms(self):
        comms_conn = comms.comms_dev_t(self.DEFAULT_COMMS_PTY_PATH,
                                       self.DEFAULT_PROTOCOL_PATH,
                                       logger=self._logger,
                                       log_file=self._log_file)
        fds = [comms_conn, self._vosm_conn]
        now = time.time()
        start_time = now
        end_time = start_time + (self._vosm_conn.interval_mins * 60) + 3
        count = 0
        final_dict = {}
        while now < end_time:
            r = select.select(fds, [], [], end_time-now)
            if len(r[0]):
                if self._vosm_conn in r[0]:
                    self._vosm_conn._ll.read()
                if comms_conn in r[0]:
                    count += 1
                    resp_dict = comms_conn.read_dict()
                    final_dict.update(resp_dict)
            now = time.time()
        ret = self._bool_check("Comms message count", count > 0, True)
        ret &= self._bool_check("Comms message matched", self._comms_match_cb(self.DEFAULT_COMMS_MATCH_DICT, final_dict), True)
        self._logger.info("Measurement loop test complete.")
        return ret

    def _check_wifi_serial_comms(self):
        if int(paho.__version__.split(".")[0]) > 1:
            mqtt_conn = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        else:
            mqtt_conn = mqtt.Client()
        mqtt_conn.enable_logger(self._logger)
        messages = []
        mqtt_conn.on_message = lambda client, userdata, message: messages.append(message)
        mqtt_conn.on_connect = lambda client, userdata, flags, reason_code, *args: client.subscribe("osm/00C0FFEE/measurements")

        mqtt_sch = self._wifi_conf["mqtt_sch"]
        use_websockets = mqtt_sch > 5
        use_ssl = mqtt_sch % 5 > 1
        insecure = mqtt_sch % 5 < 3

        if use_websockets:
            mqtt_conn.transport = "websockets"
            self._logger.debug("USING WEBSOCKETS")

        if use_ssl:
            if insecure:
                self._logger.debug("USING SSL NO VERIFY")
                mqtt_conn.tls_set(cert_reqs=ssl.CERT_NONE)
                mqtt_conn.tls_insecure_set(True)
            else:
                self._logger.debug(f"USING SSL WITH CA: {self._wifi_conf['ca']}")
                mqtt_conn.tls_set(ca_certs=self._wifi_conf["ca"])

        mqtt_conn.username_pw_set(self._wifi_conf["mqtt_user"], password=self._wifi_conf["mqtt_pwd"])

        mqtt_conn.connect(self._wifi_conf["mqtt_addr"], self._wifi_conf["mqtt_port"])
        now = time.monotonic()
        start_time = now
        end_time = start_time + (self._vosm_conn.interval_mins * 60) + 3
        final_dict = {}
        msg = b""
        while now < end_time:
            self._vosm_conn.drain(0)
            mqtt_conn.loop()
            now = time.monotonic()
        mqtt_conn.disconnect()
        mqtt_conn.loop()
        count = len(messages)
        self._logger.debug(f"Messages: = {messages}")
        for m in messages:
            self._logger.debug(f"{m.payload = }")
            self._logger.debug(f"{json.loads(m.payload) = }")
            final_dict.update(json.loads(m.payload)["VALUES"])
        self._logger.debug(f"{final_dict = }")
        ret = self._bool_check("Comms message count", count > 0, True)
        ref_dict = self.DEFAULT_COMMS_MATCH_DICT
        ref_dict.update({
            "HUMI"    : 50.18,
            "HUMI_min": 50.18,
            "HUMI_max": 50.18,
            "PF"      : 1002,
        })
        ret &= self._bool_check("Comms message matched", self._comms_match_cb(ref_dict, final_dict), True)
        self._logger.info("Measurement loop test complete.")
        return ret

    def _comms_match_cb(self, ref:dict, dict_:dict)->bool:
        passed = True
        for key, value in ref.items():
            comp = dict_.get(key, None)
            passed &= self._threshold_check(key, key, comp, value, 0.1)
        return passed

    def run(self):
        self._logger.info("Starting Virtual OSM Test...")

        self._start_osm_env()

        self._logger.info("Waiting for Keyboard interrupt...")
        try:
            while not self._done:
                time.sleep(0.5)
        except KeyboardInterrupt:
            self._logger.debug("Caught Keyboard interrupt, exiting.")

    def _wait_for_line(self, stream, pattern, timeout=3):
        start = time.monotonic()
        while start + timeout >= time.monotonic():
            line = stream.readline().decode().replace("\n", "").replace("\r", "")
            if not len(line):
                continue
            self._logger.debug(line)
            match = pattern.search(line)
            if match:
                return True
        return False

    def _close_virtual_osm(self):
        if self._vosm_proc is None:
            self._logger.debug("Virtual OSM not running, skip closing.")
            return
        self._logger.info("Closing virtual OSM.")
        self._vosm_proc.send_signal(signal.SIGINT)
        self._vosm_proc.poll()
        self._logger.debug("Sent close signal to thread. Waiting to close...")
        count = 0
        while True:
            try:
                self._vosm_proc.wait(2)
                break
            except subprocess.TimeoutExpired:
                self._logger.debug("Timeout expired... retrying sending signal.")
                self._vosm_proc.send_signal(signal.SIGINT)
                self._vosm_proc.poll()
            if count >= 4:
                self._logger.critical("Failed to close virtual OSM. %d"% count)
                return
            count += 1
        self._logger.debug("Closed virtual OSM.")
        self._vosm_proc = None

    def _spawn_virtual_osm(self, path):
        self._logger.info("Spawning virtual OSM.")
        command = self.DEFAULT_VALGRIND.split() + self.DEFAULT_VALGRIND_FLAGS.split() + path.split()
        debug_log = open(self.DEFAULT_OSM_BASE + "debug.log", "w")
        if os.path.exists(self.DEFAULT_OSM_CONFIG):
            os.unlink(self.DEFAULT_OSM_CONFIG)
        self._vosm_proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=debug_log)
        self._logger.debug("Opened virtual OSM.")
        pattern_str = "^----start----$"
        return bool(self._wait_for_line(self._vosm_proc.stdout, re.compile(pattern_str)))

    def _connect_osm(self, path, timeout=3):
        self._logger.info("Connecting to the virtual OSM.")
        if not self._wait_for_file(path, timeout):
            return False
        try:
            self._raw_connect_osm(path)
        except OSError as e:
            if e.errno == errno.EIO:
                self._logger.debug("IO error opening, trying again (timing?).")
                time.sleep(0.1)
                self._raw_connect_osm(path)
                return True
            self._logger.error(e)
            return False
        return True

    def _raw_connect_osm(self, path):
        self._vosm_conn = dev_t(path)
        if "DEBUG" in os.environ:
            set_debug_print(self._logger.debug)
        self._logger.debug("Connected to the virtual OSM.")

    def _disconnect_osm(self):
        if self._vosm_conn is None:
            self._logger.debug("Virtual OSM not connected, skip disconnect.")
            return
        self._logger.info("Disconnecting from the virtual OSM.")
        self._vosm_conn._serial_obj.close()
        self._logger.debug("Disconnected from the virtual OSM.")
        self._vosm_conn = None


def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description='Fake OSM test file.' )
        parser.add_argument("-r", "--run", help='Do not test, just run.', action='store_true'   )
        parser.add_argument("firmware"  , metavar="FW"      , type=str  , help="Firmware"       )
        parser.add_argument("config"    , metavar="CONF"    , type=str  , help="Config file"    )
        return parser.parse_args()

    args = get_args()

    if not os.path.exists(args.config):
        print("Config file does not exist", file=sys.stderr, flush=True)
        return -1
    with open(args.config, "r") as f:
        config = json.load(f)

    mqtt_info = {
        "websockets": config["websockets"],
        "use_ssl": config["ssl"],
        "insecure": config["insecure"],
    }
    mqtt_config = config["mqtt"]
    _set = lambda _d, _n, _x: [_d.update({_n: _x}) if _x is not None else None]
    _set(mqtt_info, "address", mqtt_config["address"])
    _set(mqtt_info, "port", mqtt_config["port"])
    _set(mqtt_info, "user", mqtt_config["user"])
    _set(mqtt_info, "password", mqtt_config["password"])
    _set(mqtt_info, "ca", mqtt_config["ca"])

    passed = False
    with test_framework_t(args.firmware, config["log_file"], mqtt_info=mqtt_info, do_ota=config["ota"]) as tf:
        if args.run:
            passed = tf.run()
        else:
            passed = tf.test()
    return 0 if passed else -1


if __name__ == "__main__":
    import sys
    sys.exit(main())
