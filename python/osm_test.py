#!/usr/bin/env python3

import os
import time
import subprocess
import logging
import signal

import binding


DEFAULT_FAKE_OSM_PATH  = "%s/../linux/build/firmware.elf"% os.path.dirname(__file__)
DEFAULT_DEBUG_PTY_PATH = "/tmp/osm/UART_DEBUG_slave"
DEFAULT_VALGRIND       = "valgrind"
DEFAULT_VALGRIND_FLAGS = "--leak-check=full"


LOG_FORMAT = '%(asctime)s.%(msecs)03dZ %(filename)s (%(process)d) [%(levelname)s]: %(message)s'
LOG_TIME_FORMAT = '%Y-%m-%dT%H:%M:%S'
logging.basicConfig(format=LOG_FORMAT, datefmt=LOG_TIME_FORMAT, level=logging.DEBUG)
logging.Formatter.converter = time.gmtime


def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description='Fake OSM test file.' )
        parser.add_argument("-f", "--fake_osm"           , help='Fake OSM', type=str, default=DEFAULT_FAKE_OSM_PATH)
        return parser.parse_args()

    args = get_args()

    logging.info("Starting Virtual OSM Test...")

    if not os.path.exists(args.fake_osm):
        logging.error("Cannot find fake OSM.")
        return -1

    logging.debug("Creating virtual OSM.")
    command = " ".join(DEFAULT_VALGRIND.split() + DEFAULT_VALGRIND_FLAGS.split() + args.fake_osm.split())
    osm_proc = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    logging.debug("Virtual OSM spun off.")

    start_time = time.monotonic()
    while time.monotonic() < start_time + 2:
        if osm_proc.poll() is not None:
            logging.error("Virtual OSM exited early.")
            return -1
    logging.debug("Virtual OSM created.")
    dev = binding.dev_t(DEFAULT_DEBUG_PTY_PATH)
    logging.debug("Connected to virtual OSM.")
    binding.set_debug_print(logging.debug)

    logging.info(dev.temp.value)
    logging.info(dev.humi.value)
    logging.info(dev.cc.value)

    dev._serial_obj.close()
    logging.debug("Closed serial object.")
    osm_proc.send_signal(signal.SIGINT)
    logging.debug("Sent close signal to thread. Waiting to close...")
    while osm_proc.poll() is None:
        pass
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
