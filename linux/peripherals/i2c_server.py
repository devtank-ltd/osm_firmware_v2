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

Parameters required for I2C comms:
  * slave address (7bits),
  * direction - read/write (1bit),
  * command code (8bits),
  * data (2 x 8 bits).

Data is only required when writing.

To emulate this, the following format will be sent (in hex):
slave_addr:direction:cmd_code:[data[0],data[1]]
ie. reading from veml7700 ALS reg
10:1:04:[--]
ie. writing 0x1213 to veml7700 CONF0 reg
10:0:00:[12,13]

The response the server will supply to the client read command will
follow the same format.
i.e. From the read command above, a reading of 0x5432
10:1:04:[54,32]

The response for a write will be an ack, an ack will again follow the
same format.
i.e
10:0:00:[--]
"""

I2C_MESSAGE_PATTERN_STR = "^(?P<addr>[0-9a-fA-F]{2})"\
                          "\:(?P<dir>[01]{1})"\
                          "\:(?P<cmd_code>[0-9a-fA-F]{2})"\
                          "\:\[(((?P<data_1>[0-9a-fA-F]{2}),(?P<data_2>[0-9a-fA-F]{2}))|(--))\]$"
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
                 0x04: 0x0555,
                 0x05: 0x0444,
                 0x06: 0x0333}


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

    def read(self, cmd_code):
        data = self._cmds.get(cmd_code, None)
        if data is None:
            return False
        data_1 = (data >> 8) & 0xFF
        data_2 = data & 0xFF
        return {"addr"    : self._addr,
                "dir"     : I2C_READ_NUM,
                "cmd_code": cmd_code,
                "data_1"  : data_1,
                "data_2"  : data_2}

    def write(self, cmd_code, data_1, data_2):
        cmd = self._cmds.get(cmd_code, None)
        if cmd is None:
            return False
        data = ((data_1 & 0xFF) << 8) + (data_2 & 0xFF)
        self._cmds[cmd_code] = data
        return {"addr"    : self._addr,
                "dir"     : I2C_WRIE_NUM,
                "cmd_code": cmd_code,
                "data_1"  : None,
                "data_2"  : None}


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
            line = client.read()
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
        if data["data_1"] is None and data["data_2"] is None:
            data_str = "--"
        elif data["data_1"] is not None and data["data_2"] is not None:
            data_str = "%.02x,%.02x"% (data["data_1"], data["data_2"])
        else:
            raise TypeError(f"Confused string with data_1 = {data_1} and data_2 = {data_2}.")
        payload = "%.02x:%.01x:%.02x:[%s]"% (data["addr"], data["dir"], data["cmd_code"], data_str)
        assert self._parse_i2c(payload, True) is not False, f"Created string does not match I2C pattern. '{payload}'"
        return payload

    def _i2c_process(self, client, data):
        fd = client.fileno()
        dev = self._devices.get(data["addr"], None)
        if not dev:
            self.error("Client [fd:{%d}] requested device that this server doesn't have (0x%.02x)."% (fd, data['addr']))
            return
        if data["dir"] == I2C_WRITE_NUM:
            resp = dev.write(data["cmd_code"], data["data_1"], data["data_2"])
            if resp is False:
                self.error("Failed to write device (0x%.02x) with command code (0x%.02x)."% (data['addr'], data["cmd_code"]))
                return
            payload = self._create_i2c_payload(resp)
            self._send_to_client(client, payload)
        elif data["dir"] == I2C_READ_NUM:
            resp = dev.read(data["cmd_code"])
            if resp is False:
                self.error("Failed to read device (0x%.02x) with command code (0x%.02x)."% (data['addr'], data["cmd_code"]))
                return
            payload = self._create_i2c_payload(resp)
            self._send_to_client(client, payload)
        else:
            self.error(f"Client [fd:{fd}] requested unknown direction ({data['dir']}).")
            return

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
            return False
        dict_ = match_.groupdict()
        if not self._verify_i2c_type(dict_, "addr")      or \
           not self._verify_i2c_type(dict_, "dir")       or \
           not self._verify_i2c_type(dict_, "cmd_code"):
            self.error("Could not convert the I2C strings to data.")
            return False
        if (not out and dict_["dir"] == I2C_WRITE_NUM) or (out and dict_["dir"] == I2C_READ_NUM):
            if (not self._verify_i2c_type(dict_, "data_1")) or (not self._verify_i2c_type(dict_, "data_2")):
                self.error("Wrong data for read/write.")
                return False
        elif (not out and dict_["dir"] == I2C_READ_NUM) or (out and dict_["dir"] == I2C_WRITE_NUM):
            if (dict_.get("data_1", None) is not None) or (dict_.get("data_2", None) is not None):
                self.error("Wrong data for read.")
                return False
        return dict_

    def _close_client(self, client):
        fd = client.fileno()
        self._clients.pop(fd)
        self.info(f"Disconnecting client [fd:{fd}]")
        self._selector.unregister(client)
        client.close()

    def _send_to_client(self, client, msg):
        self.debug("Message to client [fd:%u] >> '%s'" % (client.fileno(), msg))
        if not client.write(msg.encode()):
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
    devs = {VEML7700_ADDR: i2c_device_t(VEML7700_ADDR, VEML7700_CMDS)}
    i2c_sock = i2c_server_t("/tmp/osm/i2c_socket", devs)
    i2c_sock.run_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
