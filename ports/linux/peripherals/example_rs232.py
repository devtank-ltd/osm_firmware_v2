#!/usr/bin/env python3

import os
import sys
import basetypes


class rs232_dev_t(basetypes.pty_dev_t):
    def _read_pending(self):
        msg = self._read()
        if msg == b"A":
            self._write(b"B")

def main(args):
    import argparse
    osm_loc = os.getenv("OSM_LOC", "/tmp/osm/")
    DEFAULT_VIRTUAL_RS232_PATH = os.path.join(osm_loc, "UART_RS232_slave")
    if not os.path.exists(osm_loc):
        os.mkdir(osm_loc)

    def get_args():
        parser = argparse.ArgumentParser(description='Virtual Simple RS23.' )
        parser.add_argument('pseudoterminal', metavar='PTY', type=str, nargs='?', help='The pseudoterminal for the RS232 to emulate from', default=DEFAULT_VIRTUAL_RS232_PATH)
        return parser.parse_args()

    args = get_args()

    rs232_dev = rs232_dev_t(args.pseudoterminal)
    try:
        rs232_dev.run_forever()
    except KeyboardInterrupt:
        print("rs232_dev_t : Caught keyboard interrupt.")
        return -1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
