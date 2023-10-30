#!/usr/bin/env python3

import os
import sys
import select
import serial


class at_wifi_bridge_t(object):
    def __init__(self, port_a, port_b):
        self.ports = (port_a, port_b)
        self.serials = {}
        for port in self.ports:
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
                exclusive=None)
            fd = ser.fileno()
            self.serials[fd] = ser
            basename = os.path.basename(ser.port)
            print(f"{basename:>20}:{fd}", flush=True, file=sys.stderr)
        self.done = False
        self._line = b""

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()

    def close(self):
        for fd, ser in self.serials.items():
            ser.close()
        self.serials = {}

    def _write_serial(self, ser, msg):
        fd = ser.fileno()
        basename = os.path.basename(ser.port)
        self._line += msg
        if b"\n" in self._line:
            *lines, self._line = self._line.split(b"\n")
            for line in lines:
                print(f"{basename:>20}:{fd:03} << {line}", flush=True, file=sys.stderr)
        os.write(fd, msg)

    def _read_serial(self, ser):
        fd = ser.fileno()
        char = os.read(fd, 1)
        basename = os.path.basename(ser.port)
        #print(f"{basename:>20}:{fd:03} >> {char}", flush=True, file=sys.stderr)
        for own_fd, own_ser in self.serials.items():
            if ser is own_ser:
                continue
            self._write_serial(own_ser, char)

    def run_forever(self, timeout=0.1):
        ser_list = list(self.serials.values())
        while not self.done:
            rlist, wlist, xlist = select.select(ser_list, [], [], timeout)
            for rser in rlist:
                self._read_serial(rser)


def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description='AT Wifi Bridge.')
        parser.add_argument('port_A', metavar='PA', type=str, nargs='?', help='Port A of the bridge', default="/tmp/osm/UART_COMMS_slave")
        parser.add_argument('port_B', metavar='PB', type=str, nargs='?', help='Port B of the bridge', default="/dev/ttyUSB1")
        return parser.parse_args()

    args = get_args()

    with at_wifi_bridge_t(args.port_A, args.port_B) as bridge:
        try:
            bridge.run_forever()
        except KeyboardInterrupt:
            pass
    return 0

if __name__ == '__main__':
    sys.exit(main())
