#!/usr/bin/env python3

import os
import re
import sys
import socket
import datetime
import selectors


"""
This program uses sockets to provide a server for the I2C communication
to be faked.

The format for the fake I2C comms shall be as below:
MASTER>>SLAVE: slave_addr:wn[w,..]:rn
SLAVE>>MASTER: slave_addr:wn:rn[r,...]
"""
I2C_MESSAGE_PATTERN_STR = "^(?P<addr>[0-9a-fA-F]{2}):"\
                          "(?P<wn>[0-9]+)(\[(?P<w>([0-9a-fA-F]{2},?)+)\])?:"\
                          "(?P<rn>[0-9]+)(\[(?P<r>([0-9a-fA-F]{2},?)+)\])?$"
I2C_MESSAGE_PATTERN = re.compile(I2C_MESSAGE_PATTERN_STR)


I2C_WRITE_NUM = 0
I2C_READ_NUM  = 1


COLOUR_BLACK  = "\033[30m"
COLOUR_RED    = "\033[31m"
COLOUR_GREEN  = "\033[32m"
COLOUR_YELLOW = "\033[33m"
COLOUR_BLUE   = "\033[34m"
COLOUR_CYAN   = "\033[36m"
COLOUR_WHITE  = "\033[37m"

COLOUR_RESET  = COLOUR_WHITE


VEML7700_ADDR = 0x10
VEML7700_CMDS = {0x00: 0x0999,
                 0x01: 0x0888,
                 0x02: 0x0777,
                 0x03: 0x0666,
                 0x04: 0x7c01, # Raw light count = 380
                 0x05: 0x0444,
                 0x06: 0x0333}



HTU21D_ADDR   = 0x40
HTU21D_CMDS   = {0xE3: 0x63b87e, # Temperature = 21.590degC
                 0xE5: 0x741ee4, # Humidity = 50.160%
                 0xF3: 0x0777,
                 0xF5: 0x0666,
                 0xE6: 0x0555,
                 0xE7: 0x0444,
                 0xFE: 0x0333}


def log(file_, msg, colour=None):
    if file_ is not sys.stdout and file_.closed:
        file_.open()
    payload = f"[{datetime.datetime.isoformat(datetime.datetime.utcnow())}] {msg}\n"
    if file_ is sys.stdout and colour:
        payload = colour + payload + COLOUR_RESET
    file_.write(payload)
    file_.flush()
    return len(payload)


class i2c_device_t(object):
    def __init__(self, addr, cmds):
        self._addr = addr
        self._cmds = cmds
        self._last_cmd_code = None

    def _read(self, cmd_code):
        data_arr = []
        if cmd_code is None:
            cmd_code = self._last_cmd_code
        assert cmd_code is not None, "No command code given."
        orig_value = value_cpy = int(self._cmds[cmd_code])

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
        cmd_code = w[0]
        vals = w[1:]
        data = 0
        for i in range(0, len_w - 1):
            data += (vals[i] & 0xFF) << (8 * i)
        self._cmds[cmd_code] = data
        return None

    def transfer(self, data):
        wn = data["wn"]
        w  = None
        rn = data["rn"]
        r  = None
        cmd_code = None
        if wn:
            assert wn == len(data["w"]), f"WN is not equal to length of W. ({wn} != {len(data['w'])})"
            self._last_cmd_code = cmd_code = data["w"][0]
            if wn > 1:
                w = self._write(cmd_code, data["w"][1:])
        if rn:
            r_raw = self._read(cmd_code)
            r_raw_len = len(r_raw)
            assert rn >= r_raw_len, f"RN less than the length of R. ({rn} != {r_raw_len})"
            r = [0 for x in range(0, rn-r_raw_len)] + r_raw
        return {"addr"   : self._addr,
                "wn"     : wn,
                "w"      : w,
                "rn"     : rn,
                "r"      : r}


class i2c_client_t(object):
    def __init__(self, connection_obj):
            self._conn_obj = connection_obj
            self.read = lambda x=1024: self._conn_obj.recv(x)
            self.write = self._conn_obj.send
            self.fileno = self._conn_obj.fileno
            self.close = self._conn_obj.close


class i2c_server_t(object):
    def __init__(self, socket_loc, devs, log_file=None):
        if os.path.exists(socket_loc):
            os.unlink(socket_loc)
        self._server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._server.bind(socket_loc)
        self._server.setblocking(False)
        self._server.listen(5)

        self._selector = selectors.PollSelector()
        self._selector.register(self._server, selectors.EVENT_READ, self._new_client_callback)

        self._clients = {}

        self.fileno = self._server.fileno

        self._done = False

        log_file_obj = sys.stdout if log_file is None else open(log_file, "a")
        self._log = lambda m, c=None: log(log_file_obj, m, c)

        self.info    = lambda m : self._log(f"{{INFO}}: {m}", COLOUR_WHITE)
        self.error   = lambda m : self._log(f"{{ERROR}}: {m}", COLOUR_RED)
        self.warning = lambda m : self._log(f"{{WARNING}}: {m}", COLOUR_YELLOW)
        if os.environ.get("DEBUG", None):
            self.debug   = lambda m : self._log(f"{{DEBUG}}: {m}", COLOUR_CYAN)
        else:
            self.debug   = lambda *x : x

        self._devices = dict(devs)

        self.info(f"I2C SERVER INITIALISED WITH {len(self._devices)} DEVICES")

    def _new_client_callback(self, sock, __mask):
        conn, addr = sock.accept()
        client = i2c_client_t(conn)
        self.info("New client connection FD:%u" % client.fileno())

        self._clients[client.fileno()] = client

        self._selector.register(client,
                                selectors.EVENT_READ,
                                self._client_message)

    def _client_message(self, sock, __mask):
        fd = sock.fileno()
        client = self._clients.get(fd, None)
        if client is None:
            self.error(f"Message from client [fd:{fd}] not in connected clients list.")
            return
        while True:
            try:
                line = client.read()
            except OSError:
                self._close_client(client)
                return
            if line is None:
                self._close_client(client)
                return
            if line is False:
                self.warning(f"Tried to read from client [fd:{fd}] but failed.")
                return
            if not len(line):
                return
            line = line.decode()
            self.debug(f"Client [fd:{fd}] message << '{line}'")
            i2c_data = self._parse_i2c(line)
            if i2c_data is False:
                self.warning("Client message does not match I2C pattern.")
                continue
            self._i2c_process(client, i2c_data)

    def _create_i2c_payload(self, data):
        payload = "%.02x:%x"% (data["addr"], data["wn"])

        if data["w"] is not None:
            payload += "[%s]"% ",".join(["%.02x"% x for x in data["w"]])

        payload += ":%x"% data["rn"]

        if data["r"] is not None:
            payload += "[%s]"% ",".join(["%.02x"% x for x in data["r"]])

        assert self._parse_i2c(payload, True) is not False, f"Created string does not match I2C pattern. '{payload}'"
        return payload

    def _i2c_process(self, client, data):
        fd = client.fileno()
        dev = self._devices.get(data["addr"], None)
        if not dev:
            self.error("Client [fd:{%d}] requested device that this server doesn't have (0x%.02x)."% (fd, data['addr']))
            return
        resp = dev.transfer(data)
        payload = self._create_i2c_payload(resp)
        self._send_to_client(client, payload)

    def _verify_i2c_type(self, dict_, key):
        value = dict_.get(key, None)
        if value is None:
            return False
        try:
            value = int(value, 16)
        except ValueError:
            return False
        dict_[key] = value
        return True

    def _parse_i2c(self, line, out=False):
        match_ = I2C_MESSAGE_PATTERN.search(line)
        if not match_:
            self.debug(f"Regex search failed for '{line}'")
            return False
        dict_ = match_.groupdict()
        if not self._verify_i2c_type(dict_, "addr")      or \
           not self._verify_i2c_type(dict_, "wn")       or \
           not self._verify_i2c_type(dict_, "rn"):
            self.error("Could not convert the I2C strings to data.")
            return False

        wn = dict_["wn"]
        if wn and not out:
            w_str = dict_.get("w", None)
            if w_str is None:
                self.error("Command is to write but no data given.")
                return False
            w = [int(x, 16) for x in w_str.split(',')]
            if len(w) != wn:
                self.error("WN does not match W length.")
                return False
            dict_["w"] = w

        rn = dict_["rn"]
        if rn and out:
            r_str = dict_.get("r", None)
            if r_str is None:
                self.error("Command is to read but no to be read given.")
                return False
            r = [int(x, 16) for x in r_str.split(',')]
            if len(r) != rn:
                self.error("RN does not match R length.")
                return False
            dict_["r"] = r

        return dict_

    def _close_client(self, client):
        fd = client.fileno()
        try:
            self._clients.pop(fd)
        except KeyError:
            return
        self.info(f"Disconnecting client [fd:{fd}]")
        self._selector.unregister(client)
        client.close()

    def _send_to_client(self, client, msg):
        self.debug("Message to client [fd:%u] >> '%s'" % (client.fileno(), msg))
        try:
            resp = client.write(msg.encode())
        except BrokenPipeError:
            self._close_client(client)
            return
        if not resp:
            self._close_client(client)

    def _send_to_all_clients(self, msg):
        for client in self._clients.values():
            self._send_to_client(client, msg)

    def run_forever(self):
        try:
            while not self._done:
                events = self._selector.select(timeout=0.5)
                for key, mask in events:
                    callback = key.data
                    callback(key.fileobj, mask)
        except KeyboardInterrupt:
            print("Caught keyboard interrupt, exiting")
        finally:
            self._selector.close()


def main():
    devs = {VEML7700_ADDR: i2c_device_t(VEML7700_ADDR, VEML7700_CMDS),
            HTU21D_ADDR  : i2c_device_t(HTU21D_ADDR,   HTU21D_CMDS  )}
    i2c_sock = i2c_server_t("/tmp/osm/i2c_socket", devs)
    i2c_sock.run_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
