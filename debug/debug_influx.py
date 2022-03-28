import select
import sys
import os


sys.path.append(os.path.join(os.path.dirname(__file__), "../python"))
from binding import debug_dev_t, dev_base_t, log_t


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


def main(argv, argc):
    debug_mode = False
    if argc and argv[0] == "debug":
        debug_mode = True

    port = "/dev/ttyUSB0"
    if os.path.exists(port):
        d = dev_debug_t(port)
    else:
        print("Not connected")
        d = dev_fake_t(port)

    

    while True:
        r = select.select([d],[],[], 1)
        if len(r[0]):
            msg = d.read_msg()
            s = d.parse_msg(msg)
            if s:
                name, value = s
                print(f"{name} = {value}")


if __name__ == "__main__":
    main(sys.argv, len(sys.argv))
