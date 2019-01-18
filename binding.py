#! /usr/bin/python3
import os
import sys
import time
import serial
import weakref
import serial.tools.list_ports


_DEBUG = True if "DEBUG" in os.environ else False

class io_board_prop_t(object):
    def __init__(self, index, parent):
        self.parent = weakref.ref(parent)
        self.index  = index


class pps_t(io_board_prop_t):
    @property
    def value(self):
        parent = self.parent()
        r = parent.command("PPS %u" % self.index)
        v = r[0].split(b':')[1].strip()
        return int(v)


class input_t(io_board_prop_t):
    @property
    def value(self):
        parent = self.parent()
        r = parent.command("input %u" % self.index)
        v = r[0].split(b':')[1].strip()
        return False if v == b"OFF" else True if v == b"ON" else None


class output_t(io_board_prop_t):
    @property
    def value(self):
        parent = self.parent()
        r = parent.command("output %u" % self.index)
        v = r[0].split(b':')[1].strip()
        return False if v == b"OFF" else True if v == b"ON" else None

    @value.setter
    def value(self, v):
        parent = self.parent()
        r = parent.command("output %u %u" % self.index, int(v))
        assert r[0] == b'Set output %02u to %s' % (self.index, "ON" if v else "OFF")


class adc_t(io_board_prop_t):
    def __init__(self, index, parent):
        io_board_prop_t.__init__(self, index, parent)
        self._min_value = 0
        self._max_value = 0
        self._avg_value = 0
        self._age = 0

    def update_values(self):
        parent = self.parent()
        r = parent.command("adc %u" % self.index)
        assert len(r) == 4
        parts = [part.strip() for part in r[0].split(b':') ]
        assert parts[0] == b"ADC"
        assert int(parts[1].split(b' ')[0]) == self.index
        self._min_value = int(r[1].split(b':')[1].strip())
        self._max_value = int(r[2].split(b':')[1].strip())
        self._avg_value = float(r[3].split(b':')[1].strip())
        self._age = time.time()

    def _refresh(self):
        if not self._age or (time.time() - self._age) > 1:
            self.update_values()

    @property
    def min_value(self):
        self._refresh()
        return self._min_value

    @property
    def max_value(self):
        self._refresh()
        return self._max_value

    @property
    def avg_value(self):
        self._refresh()
        return self._avg_value



class io_board_py_t(object):
    __LOG_START_SPACER = b"============{"
    __LOG_END_SPACER   = b"}============"
    __PROP_MAP = {b"pps" : pps_t,
                  b"inputs" : input_t,
                  b"outputs" : output_t,
                  b"adcs" : adc_t}
    __READTIME = 2
    __WRITEDELAY = 0.001 # Stop flooding STM32 faster than it can deal with

    def __init__(self, dev):
        self.pps = []
        self.inputs = []
        self.outputs = []
        self.adcs = []
        self.comm = serial.Serial(
                port=dev,
                baudrate=115200,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS,
                timeout=1)
        r = self.command("count")
        m = {}
        for line in r:
            parts = line.split(b':')
            name  = parts[0].lower().strip()
            count = int(parts[1])
            for n in range(0, count):
                child = io_board_py_t.__PROP_MAP[name](n, self)
                children = getattr(self, name.decode())
                children += [child]

    def _read_line(self):
        line = self.comm.readline().strip()
        if _DEBUG:
            print("<<", line)
        return line

    def read_response(self):
        line = self._read_line()
        start = time.time()
        while line != io_board_py_t.__LOG_START_SPACER:
            line = self._read_line()
            assert time.time() - start < io_board_py_t.__READTIME

        line = self._read_line()
        data_lines = []

        while line != io_board_py_t.__LOG_END_SPACER:
            data_lines += [line]
            line = self._read_line()
            assert time.time() - start < io_board_py_t.__READTIME

        return data_lines

    def command(self, cmd):
        for c in cmd:
            self.comm.write(c.encode())
            time.sleep(io_board_py_t.__WRITEDELAY)
        self.comm.write(b'\n')
        self.comm.flush()
        if _DEBUG:
            print(">>", cmd)
        return self.read_response()


devpath = None

for p in serial.tools.list_ports.comports():
    if p.product == "STM32 STLink":
        devpath = p.device

if devpath:
    print("Found device :", devpath)
else:
    print("Not STM32 STLink found.")
    sys.exit(-1)

comm = io_board_py_t(devpath)

for pps in comm.pps:
    print("PPS", pps.index, pps.value)

for gpio_in in comm.inputs:
    print("Input", gpio_in.index, gpio_in.value)

for gpio_out in comm.outputs:
    print("Output", gpio_out.index, gpio_out.value)

for adc in comm.adcs:
    print("ADC", adc.index, adc.min_value, adc.max_value, adc.avg_value)
