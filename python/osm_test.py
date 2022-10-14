#!/usr/bin/env python3

import os
import re
import sys
import time
import errno
import subprocess
import logging
import signal
import multiprocessing

from binding import modbus_reg_t, dev_t, set_debug_print

sys.path.append("../linux/peripherals/")

import i2c_server as i2c
import modbus_server as modbus


class test_logging_formatter_t(logging.Formatter):
    GREEN       = "\033[32;20m"
    WHITE       = "\033[0m"
    GREY        = "\033[37;20m"
    YELLOW      = "\033[33;20m"
    RED         = "\033[31;20m"
    BOLD_RED    = "\033[31;1m"
    RESET       = WHITE
    FORMAT      = "%(asctime)s.%(msecs)03dZ %(filename)s:%(lineno)d (%(process)d) [%(levelname)s]: %(message)s"

    FORMATS = {
        logging.DEBUG:    GREY     + FORMAT + RESET,
        logging.INFO:     WHITE    + FORMAT + RESET,
        logging.WARNING:  YELLOW   + FORMAT + RESET,
        logging.ERROR:    RED      + FORMAT + RESET,
        logging.CRITICAL: BOLD_RED + FORMAT + RESET
    }

    def _format_colour(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        formatter.datefmt   = "%Y-%m-%dT%H:%M:%S"
        formatter.converter = time.gmtime
        return formatter.format(record)

    def _format_no_colour(self, record):
        formatter = logging.Formatter(self.FORMAT)
        formatter.datefmt   = "%Y-%m-%dT%H:%M:%S"
        formatter.converter = time.gmtime
        return formatter.format(record)

    def colour(self, enabled):
        if enabled:
            self.format = self._format_colour
        else:
            self.format = self._format_no_colour


class test_framework_t(object):

    DEFAULT_OSM_BASE = "/tmp/osm/"
    DEFAULT_OSM_CONFIG = DEFAULT_OSM_BASE + "osm.img"
    DEFAULT_DEBUG_PTY_PATH = DEFAULT_OSM_BASE + "UART_DEBUG_slave"
    DEFAULT_RS485_PTY_PATH = DEFAULT_OSM_BASE + "UART_RS485_slave"
    DEFAULT_I2C_SCK_PATH = DEFAULT_OSM_BASE + "i2c_socket"
    DEFAULT_VALGRIND       = "valgrind"
    DEFAULT_VALGRIND_FLAGS = "--leak-check=full"

    def __init__(self, osm_path, log_file=None):
        level = logging.DEBUG if "DEBUG" in os.environ else logging.INFO
        self._logger        = logging.getLogger(__name__)
        self._logger.setLevel(level)
        formatter           = test_logging_formatter_t()
        self._log_file      = log_file
        if self._log_file:
            streamhandler   = logging.FileHandler(self._log_file)
        else:
            streamhandler   = logging.StreamHandler()
        formatter.colour(self._log_file is None)
        streamhandler.setLevel(level)

        streamhandler.setFormatter(formatter)
        self._logger.addHandler(streamhandler)

        if not os.path.exists(osm_path):
            self._logger.error("Cannot find fake OSM.")
            raise AttributeError("Cannot find fake OSM.")

        self._vosm_path         = osm_path

        self._vosm_proc         = None
        self._vosm_conn         = None
        self._i2c_process       = None
        self._modbus_process    = None

        self._done              = False

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.exit()

    def exit(self):
        self._disconnect_osm()
        self._close_modbus()
        self._close_virtual_osm()
        self._close_i2c()

    def _wait_for_file(self, path, timeout):
        start_time = time.monotonic()
        while not os.path.exists(path):
            if start_time + timeout <= time.monotonic():
                self._logger.error(f'The path "{path}" does not exist.')
                return False
        return True

    def _threshold_check(self, desc, value, ref, tolerance):
        if isinstance(value, float):
            passed = abs(float(value) - float(ref)) <= float(tolerance)
        else:
            self._logger.debug(f'Invalid test argument {value} for "{desc}"')
            passed = False
        op = "=" if passed else "!="
        prefix = poxtfix = ""
        if self._log_file is None:
            prefix = test_logging_formatter_t.GREEN if passed else test_logging_formatter_t.RED
            poxtfix = test_logging_formatter_t.RESET
        self._logger.info(prefix + f'{desc} = {"PASSED" if passed else "FAILED"} ({value} {op} {ref} +/- {tolerance})' + poxtfix)
        return passed

    def test(self):
        self._logger.info("Starting Virtual OSM Test...")

        self._spawn_i2c()
        if not self._wait_for_file(self.DEFAULT_I2C_SCK_PATH, 3):
            return False
        if not self._spawn_virtual_osm(self._vosm_path):
            self.error("Failed to spawn virtual OSM.")
            return False

        if not self._connect_osm(self.DEFAULT_DEBUG_PTY_PATH):
            return False

        if not self._spawn_modbus(self.DEFAULT_RS485_PTY_PATH):
            return False

        self._vosm_conn.setup_modbus(is_bin=True)
        self._vosm_conn.setup_modbus_dev(5, "E53", True, True, [
            modbus_reg_t("Power Factor"    , 0xc56e, 3, "U32", "PF"  ),
            modbus_reg_t("Phase 1 volts"   , 0xc552, 3, "U32", "cVP1" )
            ])
        self._vosm_conn.setup_modbus_dev(1, "RIF", True, False, [
            modbus_reg_t("CurrentP1" , 0x10, 4, "F", "AP1" ),
            modbus_reg_t("CurrentP2" , 0x12, 4, "F", "AP2" )
            ])
        passed = True
        passed &= self._threshold_check("Temperature",        self._vosm_conn.temp.value, 20,  5)
        passed &= self._threshold_check("Humidity",           self._vosm_conn.humi.value, 50, 10)
        passed &= self._threshold_check("Power Factor",       self._vosm_conn.PF.value, 1000, 10)
        passed &= self._threshold_check("Voltage Phase 1",    self._vosm_conn.cVP1.value, 24001, 0)
        passed &= self._threshold_check("CurrentP1",          self._vosm_conn.AP1.value, 30100, 0)
        passed &= self._threshold_check("CurrentP2",          self._vosm_conn.AP2.value, 30200, 0)
        return passed

    def run(self):
        self._logger.info("Starting Virtual OSM Test...")

        self._spawn_i2c()
        if not self._wait_for_file(self.DEFAULT_I2C_SCK_PATH, 3):
            return False
        if not self._spawn_virtual_osm(self._vosm_path):
            self.error("Failed to spawn virtual OSM.")
            return False

        if not self._spawn_modbus(self.DEFAULT_RS485_PTY_PATH):
            return False

        try:
            while not self._done:
                time.sleep(0.5)
        except KeyboardInterrupt:
            self._logger.debug("Caught Keyboard interrupt, exiting.")

    def _wait_for_line(self, stream, pattern, timeout=3):
        start = time.monotonic()
        while start + timeout >= time.monotonic():
            line = stream.readline().decode().replace("\n", "").replace("\r", "")
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
        if os.path.exists(self.DEFAULT_DEBUG_PTY_PATH):
            os.unlink(self.DEFAULT_DEBUG_PTY_PATH)
        self._logger.info("Spawning virtual OSM.")
        command = self.DEFAULT_VALGRIND.split() + self.DEFAULT_VALGRIND_FLAGS.split() + path.split()
        debug_log = open(self.DEFAULT_OSM_BASE + "debug.log", "w")
        if os.path.exists(self.DEFAULT_OSM_CONFIG):
            os.unlink(self.DEFAULT_OSM_CONFIG)
        self._vosm_proc = subprocess.Popen(command, stdout=debug_log, stderr=subprocess.PIPE)
        self._logger.debug("Opened virtual OSM.")
        pattern_str = "^==[0-9]+== Command: .*build/firmware.elf$"
        return bool(self._wait_for_line(self._vosm_proc.stderr, re.compile(pattern_str)))

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
        self._vosm_conn.measurements_enable(False)
        self._logger.debug("Connected to the virtual OSM.")

    def _disconnect_osm(self):
        if self._vosm_conn is None:
            self._logger.debug("Virtual OSM not connected, skip disconnect.")
            return;
        self._logger.info("Disconnecting from the virtual OSM.")
        self._vosm_conn._serial_obj.close()
        self._logger.debug("Disconnected from the virtual OSM.")
        self._vosm_conn = None

    def _i2c_run(self):
        htu_dev = i2c.i2c_device_htu21d_t()
        devs = {i2c.VEML7700_ADDR: i2c.i2c_device_t(i2c.VEML7700_ADDR, i2c.VEML7700_CMDS),
                htu_dev.addr:  htu_dev}
        i2c_sock = i2c.i2c_server_t("/tmp/osm/i2c_socket", devs, logger=self._logger)
        i2c_sock.run_forever()

    def _spawn_i2c(self):
        if os.path.exists(self.DEFAULT_I2C_SCK_PATH):
            os.unlink(self.DEFAULT_I2C_SCK_PATH)
        self._logger.info("Spawning virtual I2C.")
        self._i2c_process = multiprocessing.Process(target=self._i2c_run, name="i2c_server", args=())
        self._i2c_process.start()
        self._logger.debug("Spawned virtual I2C.")

    def _close_i2c(self):
        self._logger.info("Closing virtual I2C.")
        if self._i2c_process is None:
            self._logger.debug("Virtual I2C process isn't running, skip closing.")
            return
        self._i2c_process.kill()
        self._logger.debug("Closed virtual I2C.")
        self._i2c_process = None

    def _modbus_run(self, path):
        modbus_server = modbus.modbus_server_t(path)
        modbus_server.run_forever()

    def _spawn_modbus(self, path, timeout=1):
        if not self._wait_for_file(path, timeout):
            return False

        self._logger.info("Spawning virtual MODBUS.")
        self._modbus_process = multiprocessing.Process(target=self._modbus_run, name="modbus_server", args=(path,))
        self._modbus_process.start()
        self._logger.debug("Spawned virtual MODBUS.")
        return True

    def _close_modbus(self):
        self._logger.info("Closing virtual MODBUS.")
        if self._modbus_process is None:
            self._logger.debug("Virtual MODBUS process isn't running, skip closing.")
            return
        self._modbus_process.kill()
        self._logger.debug("Closed virtual MODBUS.")
        self._modbus_process = None


def main():
    import argparse

    DEFAULT_FAKE_OSM_PATH = "%s/../linux/build/firmware.elf"% os.path.dirname(__file__)

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
