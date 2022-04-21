import select
import can
import struct
import os
import datetime

from influxdb import InfluxDBClient
from influxdb.exceptions import InfluxDBClientError

import libsocketcan
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


can_error_flags = [
("CAN_ERR_TX_TIMEOUT", 0x00000001, "TX timeout (by netdevice driver)"),
("CAN_ERR_LOSTARB"   , 0x00000002, "lost arbitration"),
("CAN_ERR_CRTL"      , 0x00000004, "controller problems"),
("CAN_ERR_PROT"      , 0x00000008, "protocol violations"),
("CAN_ERR_TRX"       , 0x00000010, "transceiver status"),
("CAN_ERR_ACK"       , 0x00000020, "received no ACK on transmission"),
("CAN_ERR_BUSOFF"    , 0x00000040, "bus off"),
("CAN_ERR_BUSERROR"  , 0x00000080, "bus error (may flood!)"),
("CAN_ERR_RESTARTED" , 0x00000100, "controller restarted"),
]


can_data_flags=[
[],[
("CAN_ERR_CRTL_RX_OVERFLOW", 0x01, "RX buffer overflow"),
("CAN_ERR_CRTL_TX_OVERFLOW", 0x02, "TX buffer overflow"),
("CAN_ERR_CRTL_RX_WARNING" , 0x04, "reached warning level for RX errors"),
("CAN_ERR_CRTL_TX_WARNING" , 0x08, "reached warning level for TX errors"),
("CAN_ERR_CRTL_RX_PASSIVE" , 0x10, "reached error passive status RX"),
("CAN_ERR_CRTL_TX_PASSIVE" , 0x20, "reached error passive status TX"),
("CAN_ERR_CRTL_ACTIVE"     , 0x40, "recovered to error active state"),
],[
("CAN_ERR_PROT_BIT",         0x01, "single bit error"),
("CAN_ERR_PROT_FORM",        0x02, "frame format error"),
("CAN_ERR_PROT_STUFF",       0x04, "bit stuffing error"),
("CAN_ERR_PROT_BIT0",        0x08, "unable to send dominant bit"),
("CAN_ERR_PROT_BIT1",        0x10, "unable to send recessive bit"),
("CAN_ERR_PROT_OVERLOAD",    0x20, "bus overload"),
("CAN_ERR_PROT_ACTIVE",      0x40, "active error announcement"),
("CAN_ERR_PROT_TX",          0x80, "error occurred on transmission"),
],[
("CAN_ERR_PROT_LOC_SOF",     0x03, "start of frame"),
("CAN_ERR_PROT_LOC_ID28_21", 0x02, "ID bits 28 - 21 (SFF: 10 - 3)"),
("CAN_ERR_PROT_LOC_ID20_18", 0x06, "ID bits 20 - 18 (SFF: 2 - 0 )"),
("CAN_ERR_PROT_LOC_SRTR",    0x04, "substitute RTR (SFF: RTR)"),
("CAN_ERR_PROT_LOC_IDE",     0x05, "identifier extension"),
("CAN_ERR_PROT_LOC_ID17_13", 0x07, "ID bits 17-13"),
("CAN_ERR_PROT_LOC_ID12_05", 0x0F, "ID bits 12-5"),
("CAN_ERR_PROT_LOC_ID04_00", 0x0E, "ID bits 4-0"),
("CAN_ERR_PROT_LOC_RTR",     0x0C, "RTR"),
("CAN_ERR_PROT_LOC_RES1",    0x0D, "reserved bit 1"),
("CAN_ERR_PROT_LOC_RES0",    0x09, "reserved bit 0"),
("CAN_ERR_PROT_LOC_DLC",     0x0B, "data length code"),
("CAN_ERR_PROT_LOC_DATA",    0x0A, "data section"),
("CAN_ERR_PROT_LOC_CRC_SEQ", 0x08, "CRC sequence"),
("CAN_ERR_PROT_LOC_CRC_DEL", 0x18, "CRC delimiter"),
("CAN_ERR_PROT_LOC_ACK",     0x19, "ACK slot"),
("CAN_ERR_PROT_LOC_ACK_DEL", 0x1B, "ACK delimiter"),
("CAN_ERR_PROT_LOC_EOF",     0x1A, "end of frame"),
("CAN_ERR_PROT_LOC_INTERM",  0x12, "intermission"),
],[
("CAN_ERR_TRX_CANH_NO_WIRE",       0x04, "CANH:0000 CANL:0100"),
("CAN_ERR_TRX_CANH_SHORT_TO_BAT",  0x05, "CANH:0000 CANL:0101"),
("CAN_ERR_TRX_CANH_SHORT_TO_VCC",  0x06, "CANH:0000 CANL:0110"),
("CAN_ERR_TRX_CANH_SHORT_TO_GND",  0x07, "CANH:0000 CANL:0111"),
("CAN_ERR_TRX_CANL_NO_WIRE",       0x40, "CANH:0100 CANL:0000"),
("CAN_ERR_TRX_CANL_SHORT_TO_BAT",  0x50, "CANH:0101 CANL:0000"),
("CAN_ERR_TRX_CANL_SHORT_TO_VCC",  0x60, "CANH:0110 CANL:0000"),
("CAN_ERR_TRX_CANL_SHORT_TO_GND",  0x70, "CANH:0111 CANL:0000"),
("CAN_ERR_TRX_CANL_SHORT_TO_CANH", 0x80, "CANH:1000 CANL:0000"),
]]


def ts():
    return datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')


def db_add_measurement(db, name, value):
    print(f"{name} = {value}")
    if value is not False:
        json = [{"measurement": name, "tags": {}, "time": ts(), "fields": {"value": float(value)}}]
        db.write_points(json)


def time_as_str(timestamp):
    localtime = time.localtime(timestamp)
    return time.strftime("%d/%m/%Y %H:%M:%S", localtime) + ("%0.3f" % (timestamp % 1.0))[1:]


def print_can_error_frame(msg):
    print("%s : CAN ERROR FRAME" % time_as_str(msg.timestamp))

    for e in can_error_flags:
        if msg.arbitration_id & e[1]:
            print("CAN ERROR ID FLAG: %s : %s" % (e[0], e[2]))
    for n in range(0, len(can_data_flags)):
        n_can_data_flags = can_data_flags[n]
        d = msg.data[n]
        for e in n_can_data_flags:
            if d == e[1]:
                print("CAN ERROR DATA FLAG: %s : %s" % (e[0], e[2]))


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
    c = can.interface.Bus(bustype='socketcan', channel=can_network)

    # Database
    database_name = "sf_debug"
    db_client = InfluxDBClient('localhost', port=8087)
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
            can_pkt = c.recv()
            if can_pkt.is_error_frame:
                print_can_error_frame(can_pkt)
            else:
                failed = False
                if len(can_pkt.data) != len(CAN_DATA_REF):
                    failed = True
                else:
                    for i in range(0, len(can_pkt.data)):
                        if CAN_DATA_REF[i] != can_pkt.data[i]:
                            failed = True
                pass_str = "PASSED" if not failed else "FAILED"
                print(f"CAN = {pass_str}")
            num_errs = libsocketcan.can_get_berr_counter(can_network)
            if num_errs is None:
                num_errs = 0
            db_add_measurement(db_client, "CAN_ERR", num_errs)


if __name__ == "__main__":
    import sys
    main(sys.argv, len(sys.argv))
