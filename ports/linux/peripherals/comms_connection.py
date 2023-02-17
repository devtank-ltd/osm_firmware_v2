#!/usr/bin/env python3

import os
import yaml
import basetypes
import subprocess


class comms_dev_t(basetypes.pty_dev_t):
    def __init__(self, pty, protocol,logger=None, log_file=None):
        super().__init__(pty, byte_parts=False, logger=logger, log_file=logger)
        self._protocol = protocol

    def _parse_line(self, line):
        msg = line.replace(b"\r", b"").replace(b"\n", b"")
        msg = msg.decode()
        command = f"js {self._protocol}".split() + ["%s"%msg]
        with subprocess.Popen(command, stdout=subprocess.PIPE) as proc:
            resp = proc.stdout.read()
        resp_dict = yaml.safe_load(resp)
        self._logger.debug(resp_dict)
        return resp_dict

    def read_dict(self):
        line = self._readline()
        if line:
            return self._parse_line(line)

    def _read_pending(self):
        self.read_dict()


def main():
    
    import argparse

    DEFAULT_VIRTUAL_COMMS_PATH = "/tmp/osm/UART_LW_slave"
    DEFAULT_PROTOCOL_PATH      = "%s/../../lorawan_protocol/debug.js"% os.path.dirname(__file__)

    def get_args():
        parser = argparse.ArgumentParser(description='Virtual COMMS connection for OSM' )
        parser.add_argument('pseudoterminal', metavar='PTY', type=str, nargs='?', help='The pseudoterminal for the comms to listen to', default=DEFAULT_VIRTUAL_COMMS_PATH)
        parser.add_argument('protocol', metavar='PCL', type=str, nargs='?', help='The JS protocol to encode/decode the comms.', default=DEFAULT_PROTOCOL_PATH)
        return parser.parse_args()

    args = get_args()

    comms = comms_dev_t(args.pseudoterminal, args.protocol)
    comms.run_forever()

    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
