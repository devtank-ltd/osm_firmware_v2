import serial
import select
import time

class dev_base_t(object):
    def __init__(self, port):
        if isinstance(port, str):
            self._serial_obj = serial.Serial(port=port,
                                             baudrate=115200,
                                             bytesize=serial.EIGHTBITS,
                                             parity=serial.PARITY_NONE,
                                             stopbits=serial.STOPBITS_ONE,
                                             timeout=0.1,
                                             xonxoff=False,
                                             rtscts=False,
                                             write_timeout=None,
                                             dsrdtr=False,
                                             inter_byte_timeout=None,
                                             exclusive=None)
        elif isinstance(port, serial.Serial):
            self._serial_obj = port
        else:
            raise Exception("Unsupport serial port argument.")

        self.hard_reset()
        
    def hard_reset(self):
        print("Bringing BOOT and RESET low")
        self._serial_obj.rts = True
        self._serial_obj.dtr = False

        time.sleep(1)
        print("Bringing RESET high")
        self._serial_obj.rts = False
        self._serial_obj.dtr = False

    def program_mode(self):
        print("Bringing RESET high and BOOT low")
        self._serial_obj.rts = False
        self._serial_obj.dtr = False
        time.sleep(1)

        print("Bringing RESET low and BOOT high")
        self._serial_obj.rts = False
        self._serial_obj.dtr = True
        time.sleep(1)

        print("Bringing RESET high and BOOT high")
        self._serial_obj.rts = True
        self._serial_obj.dtr = True

    def write(self, msg):
        self._serial_obj.write(("%s\n" % msg).encode())
        time.sleep(0.2)
        
    def read(self):
        try:
            msg = self._serial_obj.readline().decode()
        except UnicodeDecodeError:
            return None
        if msg == '':
            return None
        if len(msg) == 0:
            return None
        return msg

    def readlines(self, end_line=None, timeout=1):
        now = time.monotonic()
        end_time = now + timeout
        new_msg = None
        msgs = []
        while now < end_time:
            print(self)
            r = select.select([self._serial_obj], [], [], end_time - now)
            if not r[0]:
                print("Lines timeout")
            # Should be echo of command
            new_msg = self.read()
            if new_msg is None:
                print("NULL line read.")
            if new_msg:
                new_msg = new_msg.strip("\n\r")
            if new_msg != end_line:
                msgs += [new_msg]
            else:
                break
            now = time.monotonic()
        print(msgs)
        return msgs
        

if __name__ == '__main__':
    dev = dev_base_t("/dev/ttyUSB0")
