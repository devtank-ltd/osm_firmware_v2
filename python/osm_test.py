#!/usr/bin/env python3

import os
import time
import subprocess
import logging
import signal

import binding


class test_framework_t(object):

    DEFAULT_DEBUG_PTY_PATH = "/tmp/osm/UART_DEBUG_slave"
    DEFAULT_VALGRIND       = "valgrind"
    DEFAULT_VALGRIND_FLAGS = "--leak-check=full"

    def __init__(self, osm_path):
        self._logger        = logging.getLogger(__name__)
        self._logger.setLevel(logging.DEBUG)
        streamhandler       = logging.StreamHandler()
        streamhandler.setLevel(logging.DEBUG)
        LOG_FORMAT          = '%(asctime)s.%(msecs)03dZ %(filename)s (%(process)d) [%(levelname)s]: %(message)s'
        LOG_TIME_FORMAT     = '%Y-%m-%dT%H:%M:%S'
        formatter           = logging.Formatter(LOG_FORMAT, datefmt=LOG_TIME_FORMAT)
        formatter.converter = time.gmtime

        if not os.path.exists(osm_path):
            self._logger.error("Cannot find fake OSM.")
            raise AttributeError("Cannot find fake OSM.")

        streamhandler.setFormatter(formatter)
        self._logger.addHandler(streamhandler)

        self._vosm_path     = osm_path

        self._vosm_proc     = None
        self._vosm_conn     = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.exit()

    def exit(self):
        self._disconnect_osm()
        self._close_virtual_osm()
        exit()

    @staticmethod
    def _threshold_check(value, ref, tolerance):
        return abs(float(value) - float(ref)) <= float(tolerance)

    def test(self):
        self._logger.info("Starting Virtual OSM Test...")

        self._spawn_virtual_osm(self._vosm_path)
        self._connect_osm(self.DEFAULT_DEBUG_PTY_PATH)

        if not self._threshold_check(self._vosm_conn.temp.value, 20, 5):
            self.exit()

        self._disconnect_osm()
        self._close_virtual_osm()

    def _close_virtual_osm(self):
        if self._vosm_proc is None:
            self._logger.info("Virtual OSM not running, skip closing.")
            return;
        self._logger.info("Closing virtual OSM.")
        self._vosm_proc.send_signal(signal.SIGINT)
        self._logger.debug("Sent close signal to thread. Waiting to close...")
        self._vosm_proc.wait()
        self._logger.debug("Closed.")

    def _spawn_virtual_osm(self, path):
        self._logger.info("Spawning virtual OSM.")
        command = " ".join(self.DEFAULT_VALGRIND.split() + self.DEFAULT_VALGRIND_FLAGS.split() + path.split())
        self._vosm_proc = subprocess.Popen(command, shell=True)
        self._logger.debug("Opened virtual OSM.")

    def _connect_osm(self, path, timeout=1):
        self._logger.info("Connecting to the virtual OSM.")
        start_time = time.monotonic()
        while not os.path.exists(path):
            if start_time + timeout <= time.monotonic():
                self._logger.error("The path does not exist.")
                self.exit()
        self._vosm_conn = binding.dev_t(path)
        self._logger.debug("Connected to the virtual OSM.")

    def _disconnect_osm(self):
        if self._vosm_conn is None:
            self._logger.info("Virtual OSM not connected, skip disconnect.")
            return;
        self._logger.info("Disconnecting from the virtual OSM.")
        self._vosm_conn._serial_obj.close()
        self._logger.debug("Disconnected from the virtual OSM.")


def main():
    import argparse

    DEFAULT_FAKE_OSM_PATH = "%s/../linux/build/firmware.elf"% os.path.dirname(__file__)

    def get_args():
        parser = argparse.ArgumentParser(description='Fake OSM test file.' )
        parser.add_argument("-f", "--fake_osm"           , help='Fake OSM', type=str, default=DEFAULT_FAKE_OSM_PATH)
        return parser.parse_args()

    args = get_args()

    with test_framework_t(args.fake_osm) as tf:
        tf.test()
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
