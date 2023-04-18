#!/usr/bin/env python3

import os
import re
import sys
import socket
import datetime
import selectors

import socket_server_base as socket_server
from basetypes import i2c_device_t
from i2c_htu21d import i2c_device_htu21d_t
from i2c_veml7700 import i2c_device_veml7700_t

import command_server


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


VEML7700_ADDR = 0x10
VEML7700_CMDS = {0x00: 0x0999,
                 0x01: 0x0888,
                 0x02: 0x0777,
                 0x03: 0x0666,
                 0x04: 0x7c01, # Raw light count = 380
                 0x05: 0x0444,
                 0x06: 0x0333}


class i2c_server_t(socket_server.socket_server_t):
    def __init__(self, socket_loc, devs, log_file=None, logger=None):
        super().__init__(socket_loc, log_file=log_file, logger=logger)

        self._devices = dict(devs)

        self.info(f"I2C SERVER INITIALISED WITH {len(self._devices)} DEVICES")

        dev_list = {}
        for dev in devs.values():
            dev_list.update(dev.return_obj().format())

        self._command_resp = command_server.fake_devs_command_client_t("I2C", dev_list)
        self._selector.register(self._command_resp,
                                selectors.EVENT_READ,
                                self._command_resp.process)

    def _process(self, client, data):
        data = data.decode()
        self.debug(f"Client [fd:{client.fileno()}] message << '{data}'")
        i2c_data = self._parse_i2c(data)
        if i2c_data is False:
            self.warning("Client message does not match I2C pattern.")
            return
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


def main():
    htu_dev = i2c_device_htu21d_t()
    veml_dev = i2c_device_veml7700_t()
    devs = {veml_dev.addr : veml_dev,
            htu_dev.addr  : htu_dev}
    if not os.path.exists("/tmp/osm"):
        os.mkdir("/tmp/osm")
    i2c_loc = os.getenv("LOC")
    if not i2c_loc:
        i2c_loc = "/tmp/osm/"
    path = os.path.join(i2c_loc, "i2c_socket")
    i2c_sock = i2c_server_t(path, devs)
    i2c_sock.run_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
