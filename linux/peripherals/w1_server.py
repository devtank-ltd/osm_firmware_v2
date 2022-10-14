#!/usr/bin/env python3

import sys

import socket_server_base as socket_server


"""
This program uses sockets to provide a server for the one-wire
communication to be faked.
"""


class ds18b20_t(object):
    COMMANDS = { 0xCC : self._command_skip_rom ,
                 0x44 : self._command_conv_t   ,
                 0xBE : self._command_read_scp }
    
    def __init__(self, temp):
        self._temperature = temp
        self._qd_temperature = None

    @property
    def temperature(self):
        return self._temperature

    @temperature.setter
    def temperature(self, new_temperature):
        self._temperature = new_temperature

    def _command_skip_rom(self):
        return None

    def _command_conv_t(self):
        self._qd_temperature = self._temperature
        return None

    def _command_read_scp(self):
        return self._encode_data()

    def _encode_data(self):
        bytes_ = [0] * 9
        if self._qd_temperature is None:
            return bytes_
        else:
            bytes_[0] = 


class w1_server_t(socket_server.socket_server_t):
    def __init__(self, socket_loc, log_file=None, logger=None):
        super().__init__(socket_loc, log_file=log_file, logger=logger)

        self.info(f"W1 SERVER INITIALISED")

    def _process(self, client, data):
        print(data)


def main():
    w1_sock = w1_server_t("/tmp/osm/w1_socket")
    w1_sock.run_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
