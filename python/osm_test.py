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

from binding import modbus_reg_t, dev_t, set_debug_print

sys.path.append("../ports/linux/peripherals/")

import comms_connection as comms
import basetypes

sys.path.append("../config_gui/release/")

import modbus_db


class test_framework_t(object):

    DEFAULT_OSM_BASE        = "/tmp/osm/"
    DEFAULT_OSM_CONFIG      = DEFAULT_OSM_BASE + "osm.img"
    DEFAULT_DEBUG_PTY_PATH  = DEFAULT_OSM_BASE + "UART_DEBUG_slave"
    DEFAULT_COMMS_PTY_PATH  = DEFAULT_OSM_BASE + "UART_LW_slave"
    DEFAULT_VALGRIND        = "valgrind"
    DEFAULT_VALGRIND_FLAGS  = "--leak-check=full"
    DEFAULT_PROTOCOL_PATH   = "%s/../lorawan_protocol/debug.js"% os.path.dirname(__file__)

    DEFAULT_COMMS_MATCH_DICT = {'TEMP'    : 21.59,
                                'TEMP_min': 21.59,
                                'TEMP_max': 21.59,
                                'HUMI'    : 65.284,
                                'HUMI_min': 65.284,
                                'HUMI_max': 65.284,
                                'BAT'     : 131.071,
                                'BAT_min' : 131.071,
                                'BAT_max' : 131.071,
                                'LGHT'    : 1023,
                                'LGHT_min': 1023,
                                'LGHT_max': 1023}

    def __init__(self, osm_path, log_file=None):
        self._logger = basetypes.get_logger(log_file)
        self._log_file = log_file

        if not os.path.exists(osm_path):
            self._logger.error("Cannot find fake OSM.")
            raise AttributeError("Cannot find fake OSM.")

        self._vosm_path         = osm_path

        self._vosm_proc         = None
        self._vosm_conn         = None

        self._done              = False

        self._db                = modbus_db.modb_database_t(modbus_db.find_path() + "/../config_gui/release")

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
            ref = '"%s"' % os.popen('git log -n 1 --format="%h"').read().strip()
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
        self._logger.debug(f"DB RETRIEVE MEASUREMENT '{measurement_handle}'...")
        measurement_obj = getattr(self._vosm_conn, measurement_handle)
        resp = self._db.get_measurement_info(measurement_handle)
        if resp is None:
            self._logger.debug(f"DB '{measurement_handle}' IS NOT REGULAR, CHECK MODBUS...")
            resp = self._db.get_modbus_measurement_info(measurement_handle)
            if resp is None:
                self._logger.debug(f"DB NO DATA FOR '{measurement_handle}'")
                return (measurement_handle, measurement_obj, 0, 0)
        description, ref, threshold = resp
        data = (description, measurement_obj, ref, threshold)
        self._logger.debug(f"DB GOT MEASUREMENT '{measurement_handle}: '{data}'")
        return data

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
        self._vosm_conn.measurements_enable(False)
        self._vosm_conn.PM10.interval = 1
        self._vosm_conn.PM25.interval = 1
        self._vosm_conn.TMP2.interval = 1
        self._vosm_conn.CC1.interval = 1
        self._vosm_conn.CC2.interval = 1
        self._vosm_conn.CC3.interval = 1

        self._vosm_conn.setup_modbus(is_bin=True)
        self._vosm_conn.setup_modbus_dev(5, "E53", True, True, [
            modbus_reg_t(self._vosm_conn, "PF"     , 0xc56e, 3, "U32"),
            modbus_reg_t(self._vosm_conn, "cVP1"   , 0xc552, 3, "U32")
            ])
        self._vosm_conn.setup_modbus_dev(1, "RIF", True, False, [
            modbus_reg_t(self._vosm_conn, "AP1" , 0x10, 4, "F" ),
            modbus_reg_t(self._vosm_conn, "AP2" , 0x12, 4, "F" )
            ])
        passed = True

        for active in self._get_active_measurements():
            if active == "SND":
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

        # If the CCs are left on, measurement loop fails. TODO: Fix that
        self._vosm_conn.CC1.interval = 0
        self._vosm_conn.CC2.interval = 0
        self._vosm_conn.CC3.interval = 0

        self._logger.info("Running measurement loop...")
        self._vosm_conn.interval_mins = 1
        for sample in self.DEFAULT_COMMS_MATCH_DICT.keys():
            if not sample.endswith("_min") and not sample.endswith("_max"):
                self._vosm_conn.change_interval(sample, 1)
        self._vosm_conn.measurements_enable(True)

        match_cb = lambda x : self._comms_match_cb(self.DEFAULT_COMMS_MATCH_DICT, x)
        comms_conn = comms.comms_dev_t(self.DEFAULT_COMMS_PTY_PATH, self.DEFAULT_PROTOCOL_PATH, match_cb=match_cb, logger=self._logger, log_file=self._log_file)
        comms_conn.run_forever()
        passed &= comms_conn.passed

        return passed

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
            return;
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
            return;
        self._logger.info("Disconnecting from the virtual OSM.")
        self._vosm_conn._serial_obj.close()
        self._logger.debug("Disconnected from the virtual OSM.")
        self._vosm_conn = None


def main():
    import argparse

    DEFAULT_FAKE_OSM_PATH = "%s/../build/penguin/firmware.elf"% os.path.dirname(__file__)

    def get_args():
        parser = argparse.ArgumentParser(description='Fake OSM test file.' )
        parser.add_argument("-f", "--fake_osm", help='Fake OSM', type=str, default=DEFAULT_FAKE_OSM_PATH)
        parser.add_argument("-l", "--log_file", help='Log file', default=None)
        parser.add_argument("-r", "--run",      help='Do not test, just run.', action='store_true')
        return parser.parse_args()

    args = get_args()

    passed = False
    with test_framework_t(args.fake_osm, args.log_file) as tf:
        if args.run:
            passed = tf.run()
        else:
            passed = tf.test()
    return 0 if passed else -1


if __name__ == "__main__":
    import sys
    sys.exit(main())
