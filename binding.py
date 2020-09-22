from __future__ import print_function
import os
import sys
import time
import serial
import weakref
import serial.tools.list_ports


_debug_fnc = print if "DEBUG" in os.environ else None


def debug_print(msg):
    if _debug_fnc:
        _debug_fnc(msg)

def set_debug_print(_func):
    global _debug_fnc
    _debug_fnc = _func

def get_debug_print():
    return _debug_fnc



class io_board_prop_t(object):
    def __init__(self, index, parent):
        self.parent = weakref.ref(parent)
        self.index  = index


class pps_t(io_board_prop_t):
    @property
    def values(self):
        parent = self.parent()
        r = parent.command(b"pps %u" % self.index)
        parts = r[0].split(b':')
        pps = int(parts[1].strip())
        parts = parts[2].split()
        duty = int(parts[0]) / 10.0
        return (pps, duty)


class input_t(io_board_prop_t):
    @property
    def value(self):
        parent = self.parent()
        r = parent.command(b"input %u" % self.index)
        v = r[0].split(b':')[1].strip()
        return False if v == b"OFF" else True if v == b"ON" else None


class output_t(io_board_prop_t):
    @property
    def value(self):
        parent = self.parent()
        r = parent.command(b"output %u" % self.index)
        v = r[0].split(b':')[1].strip()
        return False if v == b"OFF" else True if v == b"ON" else None

    @value.setter
    def value(self, v):
        parent = self.parent()
        r = parent.command(b"output %u %u" % (self.index, int(v)))
        assert r[0] == b'Set output %02u to %s' % (self.index, b"ON" if v else b"OFF")


class adc_t(io_board_prop_t):
    def __init__(self, index, parent):
        io_board_prop_t.__init__(self, index, parent)
        self._min_value = 0
        self._max_value = 0
        self._avg_value = 0
        self._age = 0
        self.adc_scale  = 3.3/4095
        self.adc_offset = 0
        self.name       = None

    def refresh(self):
        parent = self.parent()
        r = parent.command(b"adc %u" % self.index)
        assert len(r) == 4
        parts = [part.strip() for part in r[0].split(b':') ]
        assert parts[0] == b"ADC"
        assert int(parts[1].split(b' ')[0]) == self.index
        raw_min_value = int(r[1].split(b':')[1].strip())
        raw_max_value = int(r[2].split(b':')[1].strip())
        parts = r[3].split(b':')[1].split(b'/')
        raw_avg_value = float(parts[0].strip()) / int(parts[1].strip())
        self._age = time.time()
        self._avg_value = raw_avg_value * self.adc_scale + self.adc_offset
        self._min_value = raw_min_value * self.adc_scale + self.adc_offset
        self._max_value = raw_max_value * self.adc_scale + self.adc_offset

    def _refresh(self):
        if not self._age or (time.time() - self._age) > 1:
            self.refresh()

    @property
    def min_value(self):
        self._refresh()
        return self._min_value

    @property
    def max_value(self):
        self._refresh()
        return self._max_value

    @property
    def value(self):
        self._refresh()
        return self._avg_value

    @property
    def values(self):
        self.refresh()
        return self._avg_value, self._min_value, self._max_value


class gpio_t(io_board_prop_t):
    def __init__(self, index, parent):
        io_board_prop_t.__init__(self, index, parent)

    OUT = b"OUT"
    IN = b"IN"
    PULLDOWN = b"DOWN"
    PULLUP = b"UP"
    PULLNONE = b"NONE"

    @property
    def value(self):
        parent = self.parent()
        r = parent.command(b"gpio %u" % self.index)
        v = r[0].split(b'=')[1].strip()
        return False if v == b"OFF" else True if v == b"ON" else None

    @value.setter
    def value(self, v):
        parent = self.parent()
        r = parent.command(b"gpio %u = %u" % (self.index, int(v)))
        r = r[0].split(b'=')[1].strip()
        assert r[0] == b'GPIO %02u = %s' % (self.index, b"ON" if v else b"OFF")


    @staticmethod
    def break_gpio_line(line):
        parts = line.split(b"=")
        line = parts[0].strip()
        value = parts[1].strip()
        info = line.split(b":")[1].split()
        return info[0], info[1], value

    @property
    def info(self):
        parent = self.parent()
        r = parent.command(b"gpio %02u"% self.index)
        assert len(r) == 1, "Incorrect number of lines returned by GPIO command."
        return self.break_gpio_line(r[0])

    def setup(self, direction, bias, value=None):
        assert direction in [gpio_t.IN, gpio_t.OUT]
        assert bias in [gpio_t.PULLDOWN, gpio_t.PULLUP, gpio_t.PULLNONE, None]
        parent = self.parent()
        bias = gpio_t.PULLNONE if bias is None else bias
        cmd = b"gpio %02u : %s %s" % (self.index, direction, bias)
        if value is not None:
            cmd += b" = %s"  b"ON" if v else b"OFF"
        r = parent.command(cmd)
        cmd = cmd.upper()
        assert len(r) == 1, "Incorrect number of lines returned by GPIO command."
        assert r[0].startswith(cmd), "GPIO new set as expected."


class uart_t(io_board_prop_t):
    def __init__(self, index, parent):
        io_board_prop_t.__init__(self, index, parent)
        self._io = None
        self._speed = None

    @property
    def speed(self):
        if self._speed is not None:
            return self._speed

        parent = self.parent()

        r = parent.command(b"uart %u" % self.index)
        assert r
        self._speed = int(r[0].split(b":")[1].strip())
        return self._speed

    @speed.setter
    def speed(self, speed):
        parent = self.parent()
        parent.command(b"uart %u %u" % (self.index, speed))
        self._speed = speed

    def io(self):
        if self._io:
            return self._io

        parent = self.parent()

        comm_port = parent.comm_port

        num = "".join([ c if c in "0123456789" else "" for c in comm_port ])

        base_name = comm_port[:-len(num)]

        own_port = base_name + str(int(num) + self.index + 1)

        # Doesn't matter what speed it is because of the USB indirection
        self._io = serial.Serial(port=own_port,
                          baudrate=115200,
                          parity=serial.PARITY_NONE,
                          stopbits=serial.STOPBITS_ONE,
                          bytesize=serial.EIGHTBITS,
                          timeout=1)
        return self._io

    def read(self, num):
        r = self.io.read(num)
        debug_print("UART%u >> %s" % (self.index, r))
        return r

    def write(self, s):
        debug_print("UART%u << %s" % (self.index, s))
        self.io.write(s)

    def readline(self):
        line = self.io.readline()
        debug_print("UART%u >> %s" % (self.index, line))
        return line

    def drain(self):
        line = self.readline()
        while len(line):
            line = self.readline()
        debug_print("UART%u Drained" % self.index)




class io_board_py_t(object):
    __LOG_START_SPACER = b"============{"
    __LOG_END_SPACER   = b"}============"
    __PROP_MAP = {b"ppss" : pps_t,
                  b"inputs" : input_t,
                  b"outputs" : output_t,
                  b"adcs" : adc_t,
                  b"gpios" : gpio_t,
                  b"uarts" : uart_t}
    __READTIME = 2
    __WRITEDELAY = 0.001 # Stop flooding STM32 faster than it can deal with
    NAME_MAP = {}

    def __init__(self, dev):
        self.ppss = []
        self.inputs = []
        self.outputs = []
        self.uarts   = []
        self.adcs = []
        self.gpios = []
        self.NAME_MAP = type(self).NAME_MAP
        self.comm_port = dev
        self.comm = serial.Serial(
                port=dev,
                baudrate=115200,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS,
                timeout=1)
        line = self.comm.readline()
        while len(line):
            line = self.comm.readline()
        debug_print("Drained")
        r = self.command(b"count")
        for line in r:
            parts = line.split(b':')
            name  = parts[0].lower().strip()
            count = int(parts[1])
            for n in range(0, count):
                child = type(self).__PROP_MAP[name](n, self)
                children = getattr(self, name.decode())
                children += [child]

    def load_cal_map(self, cal_map):
        for adc_name, adc_adj in cal_map.items():
            if adc_name in self.NAME_MAP:
                adc = getattr(self, adc_name)
                adc.name = adc_name
                adc.adc_scale  = adc_adj[0]
                adc.adc_offset = adc_adj[1]

    def use_gpios_map(self, gpios):
        for n in range(0, len(gpios)):
            self.gpios[n].setup(*gpios[n])
        r = self.command("count")
        m = {}
        for line in r:
            parts = line.split(b':')
            name  = parts[0].lower().strip()
            if name in ["inputs", "outputs"]:
                count = int(parts[1])
                for n in range(0, count):
                    child = type(self).__PROP_MAP[name](n, self)
                    children = getattr(self, name.decode())
                    children += [child]

    def __getattr__(self, item):
        debug_print(b"Binding name lookup " + item)
        getter = self.NAME_MAP.get(item, None)
        if getter:
            return getter(self)
        raise AttributeError("Attribute %s not found" % item)

    def _read_line(self):
        line = self.comm.readline().strip()
        debug_print(b">> : %s" % line)
        return line

    def read_response(self):
        line = self._read_line()
        start = time.time()
        while line != type(self).__LOG_START_SPACER:
            if (time.time() - start) > type(self).__READTIME:
                raise ValueError("Comms read took too long.")
            line = self._read_line()
            assert time.time() - start < type(self).__READTIME

        line = self._read_line()
        data_lines = []

        while line != type(self).__LOG_END_SPACER:
            if (time.time() - start) > type(self).__READTIME:
                raise ValueError("Comms read took too long.")
            data_lines += [line]
            line = self._read_line()
            assert time.time() - start < type(self).__READTIME

        return data_lines

    def command(self, cmd):
        self.comm.write(cmd)
        self.comm.write(b'\n')
        self.comm.flush()
        debug_print(b"<< " + cmd)
        return self.read_response()
