#!/usr/bin/env python3

import sys

import socket_server_base as socket_server


"""
This program uses sockets to provide a server for the one-wire
communication to be faked.
"""


class ds18b20_t(object):    
    def __init__(self, temp=25.0625, logger=None):
        self._temperature = temp
        self._qd_temperature = None
        self.COMMANDS = { 0xCC : self._command_skip_rom ,
                          0x44 : self._command_conv_t   ,
                          0xBE : self._command_read_scp }

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
        v = round(self._qd_temperature * 16)
        bytes_[0] = v & 0xFF            # T_LSB
        bytes_[1] = (v & 0xFF00) >> 8   # T_MSB
        bytes_[2] = 0                   # T_H
        bytes_[3] = 0                   # T_L
        bytes_[4] = 0b01111111          # Configuration
        bytes_[5] = 0xFF                # Reserved (FFh)
        bytes_[6] = 0                   # Reserved
        bytes_[7] = 0x10                # Reserved (10h)
        bytes_[8] = self._crc(bytes_[:8])
        return bytes_

    def _crc(self, mem):
        assert isinstance(mem, list),   "Memory should be a binary array."
        size = len(mem)

        crc = 0
        for i in range(0, size):
             byte = mem[i]
             for j in range (0, 8):
                blend = (crc ^ byte) & 0x01
                crc >>= 1
                if blend:
                    crc ^= 0x8C
                byte >>= 1
        return crc


class w1_server_t(socket_server.socket_server_t):
    def __init__(self, socket_loc, log_file=None, logger=None):
        super().__init__(socket_loc, log_file=log_file, logger=logger)

        self.info(f"W1 SERVER INITIALISED")
        self._ds18b20 = ds18b20_t(logger=logger)

    def _process(self, client, raw_data):
        data_list = list(raw_data)
        for data in data_list:
            self.debug(f"W1 << OSM [{'%02x'% data}]")

            command = self._ds18b20.COMMANDS.get(data, None)
            if command is None:
                self.debug(f"Unknown command, skipping.")
                return
            resp = command()
            if resp is not None:
                self.debug(f"W1 >> OSM [{', '.join(['%02x'%r for r in resp])}]")
                self._send_to_client(client, bytes(resp))


def main():
    w1_sock = w1_server_t("/tmp/osm/w1_socket")
    w1_sock.run_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
