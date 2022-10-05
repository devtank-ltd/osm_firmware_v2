#!/usr/bin/env python3

import os
import sys
import time
import subprocess
import logging
import signal
import multiprocessing

import binding

sys.path.append("../linux/peripherals/")

import i2c_server as i2c


class test_logging_formatter_t(logging.Formatter):
    GREY        = "\x1b[38;20m"
    YELLOW      = "\x1b[33;20m"
    RED         = "\x1b[31;20m"
    BOLD_RED    = "\x1b[31;1m"
    RESET       = "\x1b[0m"
    FORMAT      = "%(asctime)s %(filename)s:%(lineno)d (%(process)d) [%(levelname)s]: %(message)s"
    TIME_FORMAT = '%Y-%m-%dT%H:%M:%S'

    FORMATS = {
        logging.DEBUG: GREY + FORMAT + RESET,
        logging.INFO: GREY + FORMAT + RESET,
        logging.WARNING: YELLOW + FORMAT + RESET,
        logging.ERROR: RED + FORMAT + RESET,
        logging.CRITICAL: BOLD_RED + FORMAT + RESET
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


class test_framework_t(object):

    DEFAULT_DEBUG_PTY_PATH = "/tmp/osm/UART_DEBUG_slave"
    DEFAULT_I2C_SCK_PATH = "/tmp/osm/i2c_socket"
    DEFAULT_VALGRIND       = "valgrind"
    DEFAULT_VALGRIND_FLAGS = "--leak-check=full"

    def __init__(self, osm_path):
        self._logger        = logging.getLogger(__name__)
        self._logger.setLevel(logging.DEBUG)
        streamhandler       = logging.StreamHandler()
        streamhandler.setLevel(logging.DEBUG)
        formatter           = test_logging_formatter_t()
        formatter.converter = time.gmtime

        if not os.path.exists(osm_path):
            self._logger.error("Cannot find fake OSM.")
            raise AttributeError("Cannot find fake OSM.")

        streamhandler.setFormatter(formatter)
        self._logger.addHandler(streamhandler)

        self._vosm_path     = osm_path

        self._vosm_proc     = None
        self._vosm_conn     = None
        self._i2c_process   = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.exit()

    def exit(self):
        self._disconnect_osm()
        self._close_i2c()
        self._close_virtual_osm()

    def _wait_for_file(self, path, timeout):
        start_time = time.monotonic()
        while not os.path.exists(path):
            if start_time + timeout <= time.monotonic():
                self._logger.error('The path "{path}" does not exist.')
                return False
        return True

    def _threshold_check(self, desc, value, ref, tolerance):
        if isinstance(value, float):
            passed = abs(float(value) - float(ref)) <= float(tolerance)
        else:
            self._logger.debug(f'Invalid test argument {value} for "{desc}"')
            passed = False
        self._logger.info(f'{desc} = {"PASSED" if passed else "FAILED"}')

    def test(self):
        self._logger.info("Starting Virtual OSM Test...")

        self._spawn_i2c()
        if not self._wait_for_file(self.DEFAULT_I2C_SCK_PATH, 3):
            return False
        self._spawn_virtual_osm(self._vosm_path)

        if self._connect_osm(self.DEFAULT_DEBUG_PTY_PATH):
            self._threshold_check("Temp test", self._vosm_conn.temp.value, 20, 5)

        # self._disconnect_osm()
        # self._close_i2c()
        # self._close_virtual_osm()

    def _close_virtual_osm(self):
        if self._vosm_proc is None:
            self._logger.debug("Virtual OSM not running, skip closing.")
            return;
        self._logger.info("Closing virtual OSM.")
        self._vosm_proc.send_signal(signal.SIGINT)
        self._vosm_proc.poll()
        self._logger.debug("Sent close signal to thread. Waiting to close...")
        self._vosm_proc.wait()
        self._logger.debug("Closed virtual OSM.")
        self._vosm_proc = None

    def _spawn_virtual_osm(self, path):
        if os.path.exists(self.DEFAULT_DEBUG_PTY_PATH):
            os.unlink(self.DEFAULT_DEBUG_PTY_PATH)
        self._logger.info("Spawning virtual OSM.")
        command = self.DEFAULT_VALGRIND.split() + self.DEFAULT_VALGRIND_FLAGS.split() + path.split()
        self._vosm_proc = subprocess.Popen(command)
        self._logger.debug("Opened virtual OSM.")

    def _connect_osm(self, path, timeout=3):
        self._logger.info("Connecting to the virtual OSM.")
        if not self._wait_for_file(path, timeout):
            return False
        self._vosm_conn = binding.dev_t(path)
        binding.set_debug_print(self._logger.debug)
        self._logger.debug("Connected to the virtual OSM.")
        return True

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


def main():
    import argparse

    DEFAULT_FAKE_OSM_PATH = "%s/../linux/build/firmware.elf"% os.path.dirname(__file__)

    def get_args():
        parser = argparse.ArgumentParser(description='Fake OSM test file.' )
        parser.add_argument("-f", "--fake_osm", help='Fake OSM', type=str, default=DEFAULT_FAKE_OSM_PATH)
        return parser.parse_args()

    args = get_args()

    with test_framework_t(args.fake_osm) as tf:
        tf.test()
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
