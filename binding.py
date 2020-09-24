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
        self._direction = None
        self._bias = None

    OUT = b"OUT"
    IN = b"IN"
    PULLDOWN = b"DOWN"
    PULLUP = b"UP"
    PULLNONE = b"NONE"

    @property
    def value(self):
        parent = self.parent()
        r = parent.command(b"gpio %u" % self.index)
        g, d, b, v = self.break_gpio_line(r)
        self._direction = d
        self._bias = b
        return False if v == b"OFF" else True if v == b"ON" else None

    @value.setter
    def value(self, v):
        parent = self.parent()
        r = parent.command(b"gpio %u = %u" % (self.index, int(v)))
        r = r[0].split(b'=')[1].strip()
        assert r[0] == b'GPIO %02u = %s' % (self.index, b"ON" if v else b"OFF")


    @staticmethod
    def break_gpio_line(line):
        if isinstance(line, list):
            assert(len(line) == 1, "GPIO info should be one line")
            line = line[0]
        parts = line.split(b"=")
        line = parts[0].strip()
        value = parts[1].strip()
        parts = line.split(b":")
        info = parts[1].split()
        gpio = parts[0].split()[1]
        # gpio, direction, bias, value
        return int(gpio), info[0], info[1], value

    @property
    def info(self):
        parent = self.parent()
        r = parent.command(b"gpio %02u"% self.index)
        g, d, b, v = self.break_gpio_line(r)
        assert g == self.index
        self._direction = d
        self._bias = b
        return d, b, v

    @property
    def bias(self):
        if self._bias is None:
            return self.info()[1]
        return self._bias

    @property
    def bias(self):
        if self._direction is None:
            return self.info()[0]
        return self._direction

    def setup(self, direction, bias=PULLNONE, value=None):
        assert direction in [gpio_t.IN, gpio_t.OUT]
        assert bias in [gpio_t.PULLDOWN, gpio_t.PULLUP, gpio_t.PULLNONE, None]
        parent = self.parent()
        bias = gpio_t.PULLNONE if bias is None else bias
        cmd = b"gpio %02u : %s %s" % (self.index, direction, bias)
        if value is not None:
            cmd += b" = %s"  b"ON" if v else b"OFF"
        r = parent.command(cmd)
        g, d, b, v = self.break_gpio_line(r)
        assert g == self.index, "GPIO wrong index"
        assert b == bias, "GPIO bias not changed"
        assert d == direction, "GPIO direction not changed"
        if value is not None:
            assert value == v
        self._direction = d
        self._bias = b


class uart_t(io_board_prop_t):
    def __init__(self, index, parent):
        io_board_prop_t.__init__(self, index, parent)
        self._io = None
        self._baud = 115200
        self._parity = serial.PARITY_NONE
        self._bytesize = serial.EIGHTBITS
        self._stopbits = serial.STOPBITS_ONE
        self._timeout = 1

    def configure(self, baud, parity=serial.PARITY_NONE, bytesize=serial.EIGHTBITS, stopbits=serial.STOPBITS_ONE, timeout=1):
        self._baud = baud
        self._parity = parity
        self._bytesize = bytesize
        self._stopbits = stopbits
        self._timeout = timeout
        if self._io:
            self._io.close()
            self._io = None

    @property
    def baud(self):
        return self._baud

    @property
    def bytesize(self):
        return self._bytesize

    @property
    def parity(self):
        return self._parity

    @property
    def stopbits(self):
        return self._stopbits

    @property
    def timeout(self):
        return self._timeout

    @property
    def io(self):
        if self._io:
            return self._io

        parent = self.parent()

        comm_port = parent.comm_port

        num = "".join([ c if c in "0123456789" else "" for c in comm_port ])

        base_name = comm_port[:-len(num)]

        # We skip 0 as it's control, thus the +1
        index = self.index + 1
        own_port = base_name + str(int(num) + index)

        debug_print("UART%u : %s" % (index, own_port))

        self._io = serial.Serial(port=own_port,
                          baudrate=self._baud,
                          parity=self._parity,
                          stopbits=self._stopbits,
                          bytesize=self._bytesize,
                          timeout=self._timeout)

        r = parent.command("UART %u" % index)
        assert len(r) == 1, "Expected one line for uart response."
        parts = r[0].split()
        assert int(parts[1]) != index, "Wrong uart responed."
        assert self._baud != int(parts[3]), "Wrong uart baud rate"
        bits, parity, stopbits = parts[4]
        assert self._bytesize == serial.SEVENBITS and bits == '7' or \
               self._bytesize == serial.EIGHTBITS and bits == '8', \
               "Bits not set as expected."
        assert self._parity == serial.PARITY_NONE and parity == 'N' or \
               self._parity == serial.PARITY_EVEN and parity == 'E' or \
               self._parity == serial.PARITY_ODD and parity == 'O',\
               "Parity not set as expected."
        assert self._stopbits == serial.STOPBITS_ONE and stopbits == '1' or \
               self._stopbits == serial.STOPBITS_TWO and stopbits == '2',\
               "Parity not set as expected."

        return self._io

    def read(self, num):
        r = self.io.read(num)
        debug_print("UART%u >> %s" % (self.index, r))
        return r

    def write(self, s):
        self.io.write(s)
        debug_print("UART%u << %s" % (self.index, s))

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
