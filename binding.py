from __future__ import print_function
import os
import sys
import time
import serial
import weakref
import serial.tools.list_ports


_debug_fnc = lambda *args : print(*args, file=sys.stderr) if "DEBUG" in os.environ else None


def debug_print(msg):
    if _debug_fnc:
        _debug_fnc(msg)

def set_debug_print(_func):
    global _debug_fnc
    _debug_fnc = _func

def get_debug_print():
    return _debug_fnc

def _to_bytes(s):
    if isinstance(s, bytes):
        return s
    return s.encode()



class io_board_prop_t(object):
    def __init__(self, index, parent):
        self._parent = weakref.ref(parent)
        self.index  = index
    @property
    def parent(self):
        return self._parent()


class pps_t(io_board_prop_t):
    @property
    def values(self):
        r = self.parent.command(b"pps %u" % self.index)
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
        self.unit       = None
        self.name       = None
        self.auto_refresh = True

    def _raw_to_voltage(self, v):
        debug_print("%sADC %u%s : %G" % (self.parent.log_prefix, self.index, "(%s)" % self.name if self.name else "", v))
        return v

    def refresh(self):
        if not self.auto_refresh:
            return
        r = self.parent.command(b"adc %u" % self.index)
        self.update_from_line(r)

    def update_from_line(self, r):
        assert len(r) == 4
        parts = [part.strip() for part in r[0].split(b':') ]
        assert parts[0] == b"ADC"
        assert int(parts[1].split(b' ')[0]) == self.index
        min_str = r[1].split(b':')[1].strip()
        dot = min_str.find(b".") + 1
        if dot > 0:
            while chr(min_str[dot]).isdecimal():
                dot+=1
            unit = min_str[dot:]
            raw_min_value = float(min_str.replace(unit,b""))
            raw_max_value = float(r[2].split(b':')[1].strip().replace(unit,b""))
            raw_avg_value = float(r[3].split(b':')[1].strip().replace(unit,b""))
            self._min_value = self._raw_to_voltage(raw_min_value)
            self._max_value = self._raw_to_voltage(raw_max_value)
            self._avg_value = self._raw_to_voltage(raw_avg_value)
            self.unit = unit.decode()
        else:
            raw_min_value = int(min_str)
            raw_max_value = int(r[2].split(b':')[1].strip())
            avg_line = r[3].split(b':')[1].strip()
            parts = avg_line.split(b"/")
            assert len(parts) == 2
            raw_avg_value = int(parts[0]) / float(parts[1])
            self._min_value = self._raw_to_voltage(raw_min_value)
            self._max_value = self._raw_to_voltage(raw_max_value)
            self._avg_value = self._raw_to_voltage(raw_avg_value)
            self.unit = None

    @property
    def value(self):
        self.refresh()
        return self._avg_value

    @property
    def values(self):
        self.refresh()
        return self._avg_value, self._min_value, self._max_value

    @property
    def calibration(self):
        r = self.parent.command(b"cal %u" % self.index)
        assert len(r) == 4
        parts = [part.strip() for part in r[0].split(b':') ]
        assert parts[0] == b"ADC"
        assert int(parts[1].split(b' ')[0]) == self.index
        scale = float(r[1].split(b':')[1].strip())
        offset = float(r[2].split(b':')[1].strip())
        unit = r[3].split(b':')[1].strip()
        return scale, offset, unit.decode()

    @calibration.setter
    def calibration(self, v):
        scale, offset, unit = v
        r = self.parent.command(b"cal %uS:%0.06f" % (self.index, scale))
        assert r[0] == b"Cal scale pending"
        r = self.parent.command(b"cal %uO:%0.06f" % (self.index, offset))
        assert r[0] == b"Cal offset pending"
        r = self.parent.command(b"cal %uU:%s" % (self.index, _to_bytes(unit)))
        assert r[0] == b"Cal set"


class io_t(io_board_prop_t):
    def __init__(self, index, parent):
        io_board_prop_t.__init__(self, index, parent)
        self._direction = None
        self._bias = None
        self._locked = None
        self._has_special = None
        self._as_special = None
        self._label = None

    OUT = b"OUT"
    IN = b"IN"
    PULLDOWN = b"DOWN"
    PULLUP = b"UP"
    PULLNONE = b"NONE"

    @property
    def value(self):
        r = self.parent.command(b"io %u" % self.index)
        return self.load_from_line(r)

    @value.setter
    def value(self, v):
        r = self.parent.command(b"io %u = %u" % (self.index, 1 if v else 0))
        assert len(r) == 1, "Unexpected IO set return."
        r = r[0].strip()
        assert r == b'IO %02u = %s' % (self.index, b"ON" if v else b"OFF"), "Unexpected IO set return."

    def load_from_line(self, line):
        if isinstance(line, list):
            assert len(line) == 1, "IO info should be one line"
            line = line[0]
        parts = line.split(b"=")
        line = parts[0].strip()
        if len(parts) > 1:
            value = parts[1].strip()
            value = False if value == b"OFF" else True if value == b"ON" else None
        else:
            value = None
        parts = line.split(b":")
        assert len(parts) == 2, "Unexpected IO line."
        io = parts[0].split()[1]
        assert int(io) == self.index, "IO line not for this IO."
        parts = parts[1].split()

        if parts[0] == b"USED":
            self._label = parts[1]
            self._bias = io_t.PULLNONE
            self._direction = io_t.IN
            self._has_special = True
            self._as_special = True
            self._locked = False
            return None
        elif parts[0].startswith(b"["):
            self._has_special = True
            self._locked = False
            self._as_special = False
            self._locked = False
            self._label = parts[0].strip(b"[]")
            parts = parts[1:]
        elif parts[0] == b"HS" or parts[0] == b"RL":
            self._has_special = False
            self._locked = True
            self._as_special = False
            self._label = parts[0]
            parts = parts[1:]
        else:
            self._has_special = False
            self._as_special = False
            self._locked = False
            self._label = b""

        assert parts[0] in [io_t.OUT, io_t.IN]
        assert parts[1] in [io_t.PULLDOWN, io_t.PULLUP, io_t.PULLNONE], "Invalid IO in line"

        self._direction = parts[0]
        self._bias = parts[1]

        return value

    def _get_info(self):
        r = self.parent.command(b"io %02u"% self.index)
        self.load_from_line(r)

    @property
    def label(self):
        if self._label is None:
            self._get_info()
        return self._label

    @property
    def bias(self):
        if self.special:
            return None
        if self._bias is None:
            self._get_info()
        return self._bias

    @property
    def direction(self):
        if self.special:
            return None
        if self._direction is None:
            self._get_info()
        return self._direction

    @property
    def has_special(self):
        if self._has_special is None:
            self._get_info()
        return self._has_special

    @property
    def special(self):
        if self._as_special is None:
            self._get_info()
        return self._as_special

    @special.setter
    def special(self, value):
        if not self._has_special:
            return
        if value:
            cmd = b"sio %u" % self.index
            r = self.parent.command(cmd)
            assert len(r), "Not expected special IO line."
            assert r[0] == b"IO %02u special enabled" % self.index
            self._as_special = True
        else:
            self.setup(io_t.IN)

    @property
    def is_locked(self):
        if self._locked is None:
            self._get_info()
        return self._locked

    def setup(self, direction, bias=PULLNONE, value=None):
        if direction is None:
            self.special = True
            return
        assert direction in [io_t.IN, io_t.OUT], "Invalid IO setup argument"
        assert bias in [io_t.PULLDOWN, io_t.PULLUP, io_t.PULLNONE, None], "Invalid IO setup argument"
        if self.is_locked:
            assert direction == self.direction, "Locked IO can't change direction"

        bias = io_t.PULLNONE if bias is None else bias
        cmd = b"io %02u : %s %s" % (self.index, direction, bias)
        if value is not None:
            value = bool(value)
            cmd += b" = %s" % (b"ON" if value else b"OFF")
        r = self.parent.command(cmd)
        v = self.load_from_line(r)
        assert self._direction == direction, "IO direction not changed"
        assert self._direction != io_t.IN or self._bias == bias, "IO bias not changed"
        if value is not None:
            assert value == v


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

        parent = self.parent

        comm_port = parent.comm_port

        num = "".join([ c if c in "0123456789" else "" for c in comm_port ])

        base_name = comm_port[:-len(num)]

        own_port = base_name + str(int(num) + self.index + 1)

        debug_print("%sUART%u : %s" % (parent.log_prefix, self.index, own_port))

        self._io = serial.Serial(port=own_port,
                          baudrate=self._baud,
                          parity=self._parity,
                          stopbits=self._stopbits,
                          bytesize=self._bytesize,
                          timeout=self._timeout)

        r = parent.command("uart %u" % self.index)
        assert len(r) == 1, "Expected one line for uart response."
        parts = r[0].split()
        assert int(parts[1]) == self.index, "Wrong uart responed."
        assert self._baud == int(parts[3]), "Wrong uart baud rate"
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
        debug_print("%sUART%u >> %s" % (self.parent.log_prefix, self.index, r))
        return r

    def write(self, s):
        self.io.write(s)
        debug_print("%sUART%u << %s" % (self.parent.log_prefix, self.index, s))

    def readline(self):
        line = self.io.readline()
        debug_print("%sUART%u >> %s" % (self.parent.log_prefix, self.index, line))
        return line

    def drain(self):
        # Give enough time for it to be reasonable to expect a character
        time.sleep(1 / (self._baud / 10) * 10)
        while self.io.in_waiting:
            self.io.read(self.io.in_waiting)
            time.sleep(1 / (self._baud / 10) * 10)
        debug_print("%sUART%u Drained" % (self.parent.log_prefix, self.index))




class io_board_py_t(object):
    __LOG_START_SPACER = b"============{"
    __LOG_END_SPACER   = b"}============"
    PROP_MAP = {b"ppss" : pps_t,
                b"adcs" : adc_t,
                b"ios"   : io_t,
                b"uarts" : uart_t}
    READTIME = 2
    NAME_MAP = {}

    def __init__(self, dev, dev_label = None):
        self.ppss = []
        self.inputs = []
        self.outputs = []
        self.uarts   = []
        self.adcs = []
        self.ios = []
        self.NAME_MAP = type(self).NAME_MAP
        self.comm_port = dev
        self.comm = serial.Serial(
                port=dev,
                baudrate=115200,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS,
                timeout=1)
        while self.comm.in_waiting:
            self.comm.readline(self.comm.in_waiting)
        self.log_prefix = "%s: " % dev_label if dev_label else ""
        debug_print("%sDrained" % self.log_prefix)
        r = self.command(b"count")
        for line in r:
            parts = line.split(b':')
            name  = parts[0].lower().strip()
            count = int(parts[1])
            for n in range(0, count):
                child = type(self).PROP_MAP[name](n, self)
                children = getattr(self, name.decode())
                children += [child]

        lines = self.command(b"ios")
        for line in lines:
            parts = line.split()
            io_index = int(parts[1])
            io_obj = self.ios[io_index]
            io_obj.load_from_line(line)

        for io_obj in self.ios:
            if io_obj.label.startswith(b"PPS"):
                index = int(io_obj.label[3:])
                self.ppss[index].io_obj = io_obj
            elif io_obj.label.startswith(b"UART"):
                index = int(io_obj.label[4:-3])
                if io_obj.label.endswith(b"RX"):
                    self.uarts[index].rx_io_obj = io_obj
                elif io_obj.label.endswith(b"TX"):
                    self.uarts[index].tx_io_obj = io_obj

    def use_ios_map(self, ios):
        for n in range(0, len(ios)):
            self.ios[n].setup(*ios[n])
        r = self.command(b"count")
        m = {}
        for line in r:
            parts = line.split(b':')
            name  = parts[0].lower().strip()
            if name in ["inputs", "outputs"]:
                count = int(parts[1])
                for n in range(0, count):
                    child = type(self).PROP_MAP[name](n, self)
                    children = getattr(self, name.decode())
                    children += [child]

    @property
    def enable_calibdation(self):
        r = self.command(b"en_cal")
        assert len(r) == 1, "Invalid return count from en_cal command."
        line = r[0]
        assert line.startswith(b"ADC Cals "), "Invalid response from en_cal command."
        if line.endswith(b"Enabled"):
            return True
        if line.endswith(b"Disabled"):
            return False
        assert False, "Invalid state in response from en_cal command."

    @enable_calibdation.setter
    def enable_calibdation(self, v):
        v = int(bool(v))
        r = self.command(b"en_cal %u" % v)
        assert len(r) == 1, "Invalid return count set with en_cal command."
        line = r[0]
        if line == b"Enabled ADC Cals":
            return True
        if line == b"Disabled ADC Cals":
            return False
        assert False, "Invalid state in response set with en_cal command."

    def __getattr__(self, item):
        debug_print("%sBinding name lookup %s" %(self.log_prefix, item))
        getter = self.NAME_MAP.get(item, None)
        if getter:
            return getter(self)
        raise AttributeError("Attribute %s not found" % item)

    def sync_all_adcs(self):
        r = self.command(b"adcsw")
        pos = 0
        for adc in self.adcs:
            adc.auto_refresh = False
            adc.update_from_line(r[pos:pos+4])
            pos += 4

    @property
    def cal_name(self):
        r = self.command(b"cal_name")
        assert len(r) == 1 and r[0][:2] == b"N:", "Invalid cal_name response."
        return r[0][2:].decode()

    @property
    def adcs_calibration(self):
        r = {"name" : self.cal_name}
        for adc in self.adcs:
            r[adc.index] = list(adc.calibration)
        return r

    @adcs_calibration.setter
    def adcs_calibration(self, cal):
        for k, v in cal.items():
            if k == "name":
                name = cal["name"]
                r = self.command(b"persist %s" % _to_bytes(name))
                assert (len(r) == 1 and r[0] == b"New persistent data started."), "Invalid persist response."
            else:
                self.adcs[k].calibration = tuple(v)

    def _read_line(self):
        line = self.comm.readline().strip()
        debug_print("%s>> : %s" % (self.log_prefix, line))
        return line

    def read_response(self):
        line = self._read_line()
        start = time.time()
        while line != type(self).__LOG_START_SPACER:
            if (time.time() - start) > type(self).READTIME:
                raise ValueError("Comms read took too long.")
            line = self._read_line()
            assert time.time() - start < type(self).READTIME

        line = self._read_line()
        data_lines = []

        while line != type(self).__LOG_END_SPACER:
            if (time.time() - start) > type(self).READTIME:
                raise ValueError("Comms read took too long.")
            data_lines += [line]
            line = self._read_line()
            assert time.time() - start < type(self).READTIME

        return data_lines

    def command(self, cmd):
        self.comm.write(cmd)
        self.comm.write(b'\n')
        self.comm.flush()
        debug_print("%s<< %s" % (self.log_prefix, cmd))
        return self.read_response()
