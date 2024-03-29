import os
import time
import select
import serial
import logging


class test_logging_formatter_t(logging.Formatter):
    GREEN       = "\033[32;20m"
    WHITE       = "\033[0m"
    GREY        = "\033[37;20m"
    YELLOW      = "\033[33;20m"
    RED         = "\033[31;20m"
    BOLD_RED    = "\033[31;1m"
    RESET       = WHITE
    FORMAT      = "%(asctime)s.%(msecs)03dZ %(filename)s:%(lineno)d (%(process)d) [%(levelname)s]: %(message)s"

    FORMATS = {
        logging.DEBUG:    GREY     + FORMAT + RESET,
        logging.INFO:     WHITE    + FORMAT + RESET,
        logging.WARNING:  YELLOW   + FORMAT + RESET,
        logging.ERROR:    RED      + FORMAT + RESET,
        logging.CRITICAL: BOLD_RED + FORMAT + RESET
    }

    def _format_colour(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        formatter.datefmt   = "%Y-%m-%dT%H:%M:%S"
        formatter.converter = time.gmtime
        return formatter.format(record)

    def _format_no_colour(self, record):
        formatter = logging.Formatter(self.FORMAT)
        formatter.datefmt   = "%Y-%m-%dT%H:%M:%S"
        formatter.converter = time.gmtime
        return formatter.format(record)

    def colour(self, enabled):
        if enabled:
            self.format = self._format_colour
        else:
            self.format = self._format_no_colour

def get_logger(log_file=None):
    level = logging.DEBUG if "DEBUG" in os.environ else logging.INFO
    logger        = logging.getLogger(__name__)
    logger.setLevel(level)
    formatter           = test_logging_formatter_t()
    if log_file:
        streamhandler   = logging.FileHandler(log_file)
    else:
        streamhandler   = logging.StreamHandler()
    formatter.colour(log_file is None)
    streamhandler.setLevel(level)
    streamhandler.setFormatter(formatter)
    logger.addHandler(streamhandler)
    return logger


class i2c_device_t(object):
    def __init__(self, addr, cmds, cmd_size=1):
        self.addr  = addr
        self._cmds = cmds
        self._last_cmd_code = None
        self._cmd_size = cmd_size

    def _read(self, cmd_code):
        data_arr = []
        if cmd_code is None:
            cmd_code = self._last_cmd_code
        resp = self._cmds.get(cmd_code, None)
        assert resp is not None, f"Code not in command set for 0x{self.addr:02x}:0x{cmd_code:04x} : {resp}"
        orig_value = value_cpy = int(resp)

        count = 0
        while value_cpy != 0:
            count += 1
            value_cpy = value_cpy >> 8

        for i in range(count, 0, -1):
            shift = 8 * (i - 1)
            this_value = orig_value >> shift
            data_arr.append(this_value & 0xFF)
        return data_arr

    def _write(self, cmd_code, w):
        assert isinstance(w, list), "w must be a list"
        len_w = len(w)
        assert len_w >= 1, f"w must have at least 1 element ({len_w})."
        vals = list(w)
        data = 0
        for i in range(0, len_w):
            data += (vals[i] & 0xFF) << (8 * i)
        self._cmds[cmd_code] = data
        return None

    def _data_to_cmd_code(self, data):
        cmd_code = 0
        for i in range (0, self._cmd_size):
            cmd_code <<= 8
            index = self._cmd_size - i - 1
            cmd_code += data[index]
        return cmd_code

    def transfer(self, data):
        wn = data["wn"]
        w  = None
        rn = data["rn"]
        r  = None
        cmd_code = None
        if wn:
            assert wn == len(data["w"]), f"WN is not equal to length of W. ({wn} != {len(data['w'])})"
            self._last_cmd_code = cmd_code = self._data_to_cmd_code(data["w"])
            if wn > self._cmd_size:
                w = self._write(cmd_code, data["w"][self._cmd_size:])
        if rn:
            r_raw = self._read(cmd_code)
            r_raw_len = len(r_raw)
            assert rn >= r_raw_len, f"RN less than the length of R. ({rn} != {r_raw_len})"
            r = [0 for x in range(0, rn-r_raw_len)] + r_raw
        return {"addr"   : self.addr,
                "wn"     : wn,
                "w"      : w,
                "rn"     : rn,
                "r"      : r}
    """
            if self._byte_parts:
                m = self._read()
                self._parse_in(m)
            else:
                line = self._readline()
                self._parse_line(line)
    """

    def return_obj(self):
        raise NotImplementedError

class pty_dev_t(object):
    def __init__(self, pty, byte_parts=False, logger=None, log_file=None):
        if logger is None:
            logger = get_logger(log_file)
        self._logger = logger
        self._serial_obj = serial.Serial(port=pty)
        self.fileno = self._serial_obj.fileno
        self._done = False
        self._byte_parts = byte_parts

    def run_forever(self, timeout=3):
        while not self._done:
            r = select.select([self], [], [], timeout)
            if not r[0]:
                continue
            self._read_pending()

    def _read_pending(self):
        raise NotImplemented

    def _read(self):
        data = self._serial_obj.read()
        self._logger.debug(f"PTY << OSM {data}")
        return data

    def _readline(self):
        data = self._serial_obj.readline()
        self._logger.debug(f"PTY << OSM {data}")
        return data

    def _write(self, data):
        self._logger.debug(f"PTY >> OSM {data}")
        r = self._serial_obj.write(data)
        self._serial_obj.flush()
        assert r == len(data), "Written length does not equal queued length."
        return r
