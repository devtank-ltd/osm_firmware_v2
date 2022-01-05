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



class io_board_py_t(object):
    __LOG_START_SPACER = b"============{"
    __LOG_END_SPACER   = b"}============"
    PROP_MAP = {b"ios"   : io_t}
    READTIME = 2
    NAME_MAP = {}

    def __init__(self, dev, dev_label = None):
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

    def __getattr__(self, item):
        debug_print("%sBinding name lookup %s" %(self.log_prefix, item))
        getter = self.NAME_MAP.get(item, None)
        if getter:
            return getter(self)
        raise AttributeError("Attribute %s not found" % item)

    def fw_upload(self, filmware):
        from crccheck.crc import CrcModbus
        data = open(filmware, "rb").read()
        crc = CrcModbus.calc(data)
        hdata = [b"%02x" % x for x in data]
        hdata = b"".join(hdata)
        mtu=56
        for n in range(0, len(hdata), mtu):
            debug_print("Chunk %u/%u" % (n/2, len(hdata)/2))
            chunk=hdata[n:n + mtu]
            r = self.command(b"fw+ "+chunk)
            expect= b"FW %u chunk added" % (len(chunk)/2)
            assert expect in r
        r = self.command(b"fw@ %04x" % crc)
        assert b"FW added" in r

    def reset(self):
        self.comm.write(b'reset\n')
        self.comm.flush()
        while True:
            line = self._read_line()
            if b'----start----' in line:
                return

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
