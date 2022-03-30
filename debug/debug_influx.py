import select
import socket
import struct
import sys
import os
import datetime

from influxdb import InfluxDBClient

sys.path.append(os.path.join(os.path.dirname(__file__), "../python"))
from binding import dev_debug_t, dev_base_t, log_t


class low_level_fake_t(object):
    def __init__(self, log_obj):
        self._log_obj = log_obj

    def fileno(self):
        return 0


class dev_fake_t(dev_base_t):
    def __init__(self, port="/dev/ttyUSB0"):
        self._log_obj = log_t("PC", "OSM")
        self._log = self._log_obj.emit
        self._ll = low_level_fake_t(self._log_obj)
        self.fileno = self._ll.fileno

    def write(self, msg):
        self._log_obj.send(msg)
        time.sleep(0.1)

    def read(self):
        return None

    def readlines(self):
        msg = self.read()
        msgs = []
        while msg != None:
            msgs.append(msg)
            msg = self.read()
        return msgs


def ts():
    return datetime.datetime.now().strftime('%Y-%m-%dT%H:%M:%SZ')


def db_add_measurement(db, name, value):
    print(f"{name} = {value}")
    json = [{"measurement": name, "tags": {}, "time": ts(), "fields": {"value": value}}]
    db.write_points(json)


CAN_DATA_REF = [1,2,3,4,5,6]


def main(argv, argc):
    debug_mode = False
    if argc and argv[0] == "debug":
        debug_mode = True

    # Serial
    port = "/dev/ttyUSB0"
    if os.path.exists(port):
        d = dev_debug_t(port)
    else:
        print("Not connected")
        d = dev_fake_t(port)

    # CAN
    can_network = "can0"
    c = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    c.bind((can_network,))

    # Database
    database_name = "sf_debug"
    db_client = InfluxDBClient('localhost', port=8086)
    db_client.switch_database(database_name)

    while True:
        r = select.select([d, c],[],[], 1)
        if d in r[0]:
            msgs = d.read_msgs()
            if msgs:
                for t in msgs:
                    name, value = t
                    db_add_measurement(db_client, name, value)
        elif c in r[0]:
            can_pkt = c.recv(16)
            base_fmt = "<IB3x"
            base_len = struct.calcsize(base_fmt)
            can_id, length = struct.unpack(base_fmt, can_pkt[:base_len])
            fmt = base_fmt + "B" * length
            fmt_len = struct.calcsize(fmt)
            failed = False
            if len(data) != len(CAN_DATA_REF):
                failed = True
            else:
                for i in range(0, len(data)):
                    if CAN_DATA_REF[i] != data[i]:
                        failed = True
            pass_str = "PASSED" if not failed else "FAILED"
            print(f"CAN = {pass_str}")

            # db_add_measurement(db_client, "CAN", 1)


if __name__ == "__main__":
    main(sys.argv, len(sys.argv))
