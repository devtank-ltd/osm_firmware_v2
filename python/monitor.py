#!/usr/bin/env python3

# Currently wont exit with Ctrl+C, need to send SIGINT

import os
import sys
import pty
import select
import serial


def _pty_bridge(port):
    ser = serial.Serial(
        port=port,
        baudrate=115200,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.1,
        xonxoff=False,
        rtscts=False,
        write_timeout=None,
        dsrdtr=False,
        inter_byte_timeout=None,
        exclusive=None
    )

    ### RESET:0 BOOT0:0 ###
    ser.setRTS(1)
    ser.setDTR(0)
    ### RESET:1 BOOT0:0 ###
    ser.setRTS(0)
    ser.setDTR(0)

    while True:
        rlist,wlist,xlist = select.select([ser, sys.stdin], [], [], 10)
        for src in rlist:
            if src is ser:
                dst = sys.stdout
            else:
                dst = ser
            m = os.read(src.fileno(), 1)
            os.write(dst.fileno(), m)

def main(args):
    port = args[0]
    pty.spawn(sys.executable, lambda fd: _pty_bridge(port))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
