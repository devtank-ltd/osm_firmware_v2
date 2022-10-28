#!/usr/bin/env python3

import time
import select
import serial

import basetypes


class pty_dev_t(object):
    def __init__(self, pty, logger=None, log_file=None):
        if logger is None:
            logger = basetypes.get_logger(log_file)
        self._logger = logger
        self._serial_obj = serial.Serial(port=pty)
        self.fileno = self._serial_obj.fileno
        self._done = False

    def run_forever(self, timeout=3):
        while not self._done:
            r = select.select([self], [], [], timeout)
            if not r[0]:
                continue
            m = self._read()
            self._parse_in(m)

    def _parse_in(self, m):
        raise NotImplemented

    def _read(self):
        data = self._serial_obj.read()
        self._logger.debug(f"PTY << OSM {data}")
        return data

    def _write(self, data):
        self._logger.debug(f"PTY >> OSM {data}")
        r = self._serial_obj.write(data)
        self._serial_obj.flush()
        assert r == len(data), "Written length does not equal queued length."
        return r


class hpm_dev_t(pty_dev_t):
    def __init__(self, pty, logger=None, log_file=None, pm2_5=20, pm10=30):
        super().__init__(pty, logger=logger, log_file=log_file)
        self._pm2_5    = pm2_5
        self._pm10     = pm10
        self._on        = False
        self._logger.info("INITIALISED VIRTUAL HPM")

        self._msg       = b""

    def _parse_in(self, m):
        self._msg += m
        if len(self._msg) > 4:
            self._msg = self._msg[-4:]
        if self._parse_msg(self._msg):
            self._msg = b""

    def _parse_msg(self, msg):
        self._logger.debug(f"Parsing {msg}")
        if msg == b"ON\n":
            self._on = True
            self._logger.debug("Turning on the HPM.")
            self._send_hpm()
            return True
        elif msg == b"OFF\n":
            self._on = False
            self._logger.debug("Turning off the HPM.")
            return True
        elif msg == bytes([0x68, 0x01, 0x04, 0x93]):
            self._logger.debug("Requested HPM values.")
            self._send_hpm()
            return True
        return False

    def _send_hpm(self):
        self._logger.debug(f"Sending HPM values. (PM2.5 = {self._pm2_5}, PM10 = {self._pm10})")
        head = 0x40
        len_ = 0x05
        cmd  = 0x04
        df1 = int(self._pm2_5   / 256.) & 0xFF
        df2 = (int(self._pm2_5) % 256 ) & 0xFF
        df3 = int(self._pm10    / 256.) & 0xFF
        df4 = (int(self._pm10)  % 256 ) & 0xFF
        cs  = ((0x10000 - (head + len_ + cmd + df1 + df2 + df3 + df4)) % 256) & 0xFF
        payload = [head, len_, cmd, df1, df2, df3, df4, cs]
        size = len(payload)
        sent_size = self._write(bytes(payload))
        assert sent_size == size, f"Sent payload not equal to contructed payload. ({sent_size} != {size})"


def main():
    import argparse

    DEFAULT_VIRTUAL_HPM_PATH = "/tmp/osm/UART_HPM_slave"

    def get_args():
        parser = argparse.ArgumentParser(description='Virtual HPM.' )
        parser.add_argument('pseudoterminal', metavar='PTY', type=str, nargs='?', help='The pseudoterminal for the HPM to emulate from', default=DEFAULT_VIRTUAL_HPM_PATH)
        return parser.parse_args()

    args = get_args()

    hpm = hpm_dev_t(args.pseudoterminal)
    try:
        hpm.run_forever()
    except KeyboardInterrupt:
        print("Caught keyboard interrupt.")
        return -1

    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
