#!/usr/bin/env python3

import os
import sys
import tty
import time
import enum
import select
import serial
import weakref
import multiprocessing


class base_at_commands_t(object):
    OK          = "OK"
    PARAM_ERROR = "AT+ERROR: PARAM"
    STATE_ERROR = "AT+ERROR: STATE"

    TEST    = "TEST"
    QUERY   = "QUERY"
    SET     = "SET"
    EXECUTE = "EXECUTE"

    def __init__(self, device, command_controller, commands: dict):
        self.COMMAND_TYPES = enum.Enum(
            "COMMAND_TYPES", [
                self.TEST,
                self.QUERY,
                self.SET,
                self.EXECUTE,
            ])
        self._device = weakref.ref(device)
        self._command_controller = weakref.ref(command_controller)
        self.commands = commands

    @property
    def device(self):
        return self._device()

    @property
    def command_controller(self):
        return self._command_controller()

    def reply(self, reply: bytes):
        return self.command_controller.reply(reply)

    def reply_ok(self):
        return self.reply(self.OK)

    def reply_param_error(self):
        return self.reply(self.PARAM_ERROR)

    def reply_state_error(self):
        return self.reply(self.STATE_ERROR)


class at_wifi_basic_at_commands_t(base_at_commands_t):
    def __init__(self, device, command_controller):
        commands = \
        {
            b"AT"                   : { base_at_commands_t.EXECUTE:     self.ping},     # Test at Startup
            b"AT+RST"               : { base_at_commands_t.EXECUTE:     self.reset},    # Restart a module
            b"AT+GMR"               : { base_at_commands_t.EXECUTE:     self.version},  # Check version information
            b"AT+CMD"               : { base_at_commands_t.QUERY:       self.list},     # List all AT commands and types supported in current firmware
            b"AT+RESTORE"           : { base_at_commands_t.EXECUTE:     self.restore},  # Restore factory default settings of the module
        }
        super().__init__(device, command_controller, commands)

    def ping(self, args):
        self.reply_ok()

    def reset(self, args):
        self.reply_ok()
        self.device.restart()

    def version(self, args):
        self.reply(self.device.version)
        self.reply_ok()

    def list(self, args):
        lines = []
        for index, (cmd_name, funcs) in enumerate(self.command_controller.commands.items()):
            support_test_command    = 1 if funcs.get(base_at_commands_t.TEST)    else 0
            support_query_command   = 1 if funcs.get(base_at_commands_t.QUERY)   else 0
            support_set_command     = 1 if funcs.get(base_at_commands_t.SET)     else 0
            support_execute_command = 1 if funcs.get(base_at_commands_t.EXECUTE) else 0
            line = f"+CMD:{index},{cmd_name.decode()},{support_test_command},{support_query_command},{support_set_command},{support_execute_command}"
            lines.append(line.encode())
        lines_str = self.device.EOL.join(lines)
        self.reply(lines_str)
        self.reply_ok()

    def restore(self, args):
        self.reply_ok()
        self.device.restore()
        self.device.restart()


class at_wifi_at_commands_t(base_at_commands_t):
    def __init__(self, device, command_controller):
        commands = \
        {
            b"AT+CWINIT"            : { base_at_commands_t.QUERY:       self.is_init,
                                        base_at_commands_t.SET:         self.init},         # Initialize/Deinitialize Wi-Fi driver
            b"AT+CWSTATE"           : { base_at_commands_t.EXECUTE:     self.info},         # Query the Wi-Fi state and Wi-Fi information
            b"AT+CWJAP"             : { base_at_commands_t.QUERY:       self.is_connect,
                                        base_at_commands_t.SET:         self.connect_info,
                                        base_at_commands_t.EXECUTE:     self.connect},      # Connect to an AP
            b"AT+CWQAP"             : { base_at_commands_t.EXECUTE:     self.disconnect},   # Disconnect from an AP
        }
        super().__init__(device, command_controller, commands)

    def is_init(self, args):
        self.reply(b"+CWINIT:%u"% (1 if self.device.wifi.init else 0))
        self.reply_ok()

    def init(self, args):
        try:
            val = int(args)
        except ValueError:
            self.reply_param_error()
            return
        if val < 0 or val > 1:
            self.reply_param_error()
            return
        self.device.wifi.init = bool(val)
        self.reply_ok()

    def info(self, args):
        info = f"+CWSTATE:{self.device.wifi.state.value},{self.device.wifi.ssid}"
        self.reply(info.encode())
        self.reply_ok()

    def is_connect(self, args):
        wifi = self.device.wifi
        if wifi.state == wifi.STATES.NOT_CONN or wifi.state == wifi.STATES.DISCONNECTED:
            self.reply_state_error()
            return
        info = f"CWJAP:{wifi.ssid},{wifi.bssid},<channel>,<rssi>,{wifi.pci_en},{wifi.reconn_interval},{wifi.listen_interval},{wifi.scan_mode},{wifi.pmf}"
        self.reply(info.encode())
        self.reply_ok()

    def connect_info(self, args):
        raise NotImplementedError

    def connect(self, args):
        wifi = self.device.wifi
        if not wifi.init or not len(wifi.ssid):
            self.reply_state_error()
            return
        self.device.wifi_connect(True)
        self.reply(b"WIFI CONNECTED")
        self.reply(b"WIFI GOT IP")
        self.reply_ok()

    def disconnect(self, args):
        wifi = self.device.wifi
        if wifi.state != wifi.STATES.CONNECTED and wifi.state != wifi.STATES.CONNECTING and wifi.state != wifi.STATES.NO_IP:
            self.reply_state_error()
            return
        self.device.wifi_connect(False)
        self.reply_ok()


class at_wifi_at_commands_t(object):

    NO_COMMAND      = "AT+ERROR: CMD"
    INVALID_TYPE    = "AT+ERROR: TYPE"

    def __init__(self, device, types:list):
        self._device = weakref.ref(device)
        self.commands = {}
        for type_ in types:
            new_obj = type_(device, self)
            self.commands.update(new_obj.commands)

    @property
    def device(self):
        return self._device()

    def reply(self, reply: bytes):
        return self.device.send(reply)

    def _no_command(self):
        self.reply(self.NO_COMMAND)

    def _invalid_type(self):
        self.reply(self.INVALID_TYPE)

    def command(self, command):
        # Get type of function
        type_ = base_at_commands_t.EXECUTE
        keyword = command
        args = b""
        if b"=" in command:
            # Is set
            type_ = base_at_commands_t.SET
            keyword,args = command.split(b"=")
        elif command.endswith(b"?"):
            type_ = base_at_commands_t.QUERY
            keyword,*_ = command.split(b"?")
            args = b""
        funcs = self.commands.get(keyword)
        if funcs:
            cb = funcs.get(type_)
            if cb:
                cb(args)
            else:
                self._invalid_type()
        else:
            # Not found in commands
            self._no_command()


class at_wifi_t(object):

    STATES = enum.Enum(
        "STATES", [
            "NOT_CONN",
            "NO_IP",
            "CONNECTED",
            "CONNECTING",
            "DISCONNECTED",
        ])


    def __init__(self):
        self.init               = False
        self.mode               = 3 # Auto
        self.ssid               = ""
        self.pwd                = ""
        self.bssid              = ""
        self.pci_en             = ""
        self.reconn_interval    = 0
        self.listen_interval    = 3
        self.scan_mode          = 1
        self.jap_timeout        = 15
        self.pmf                = 1
        self.state              = self.STATES.NOT_CONN


class at_wifi_dev_t(object):

    EOL             = b"\r\n"
    AT_VERSION      = "AT version (FAKE):2.2.0.0"
    SDK_VERSION     = "v4.0.1 (FAKE)"
    COMPILE_TIME    = "compile time:Oct 16 2023 12:18:22"
    BIN_VERSION     = "2.1.0 (FAKE)"

    def __init__(self, port):
        self.port = port

        self._serial = os.fdopen(port, "rb", 0)

        tty.setraw(self._serial)

        self.done = False
        self._serial_in = b""
        self.is_echo = False
        self.wifi = at_wifi_t()

        command_types = [
            at_wifi_basic_at_commands_t,
            at_wifi_at_commands_t,
            ]
        self._command_obj = at_wifi_at_commands_t(self, command_types)
        print("INITED")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()

    def close(self):
        if self._serial:
            self._serial.close()
            self._serial = None

    def send(self, msg:bytes):
        if isinstance(msg, str):
            msg = msg.encode()
        if not msg.endswith(self.EOL):
            msg += self.EOL
        return os.write(self._serial.fileno(), msg)

    def _do_command(self, line):
        self._command_obj.command(line)

    def _read_serial(self, fd):
        char = os.read(fd, 1)
        if self.is_echo:
            os.write(fd, char)
        self._serial_in += char
        eol = b'\r' # Not sure why this isn't self.EOL \r\n
        if self._serial_in.endswith(eol):
            line = self._serial_in[:-len(eol)]
            self._do_command(line)
            self._serial_in = b""

    def run_forever(self):
        r_fd_cbs = { self._serial.fileno() : self._read_serial }
        while not self.done:
            rlist,wlist,xlist = select.select(list(r_fd_cbs.keys()), [], [], 1)
            for r in rlist:
                cb = r_fd_cbs.get(r, None)
                fd = r if isinstance(r, int) else r.fileno()
                if cb:
                    cb(fd)

    def restart(self):
        raise NotImplementedError

    @property
    def version(self):
        version = self.AT_VERSION.encode()  + self.EOL \
            + self.SDK_VERSION.encode()     + self.EOL \
            + self.COMPILE_TIME.encode()    + self.EOL \
            + self.BIN_VERSION.encode()     + self.EOL
        return version

    def restore(self):
        raise NotImplementedError

    def wifi_connect(self, connect:bool):
        self.wifi.state = wifi.CONNECTED if connect else wifi.DISCONNECTED


def run_fake(tty_path):
    directory = os.path.dirname(tty_path)
    if not os.path.exists(directory):
        os.mkdir(directory)
    if os.path.islink(tty_path):
        a = os.access(tty_path, os.F_OK)
        if a:
            print("Symlink already exists")
            return -1
        os.unlink(tty_path)
    master, slave = os.openpty()
    os.symlink(os.ttyname(slave), tty_path)
    print(f"Connect to: {tty_path}")
    port = os.ttyname(master)
    dev = at_wifi_dev_t(master)
    try:
        dev.run_forever()
    except KeyboardInterrupt:
        print("\r", flush=True, file=sys.stderr)
    os.unlink(tty_path)
    return 0


def main(args):
    directory = "/tmp/fake_at_wifi"
    tty_name = "tty"
    tty_path = os.path.join(directory, tty_name)
    return run_fake(tty_path)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
