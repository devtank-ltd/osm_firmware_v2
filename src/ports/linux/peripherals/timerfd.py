#!/usr/bin/env python


from ctypes import cdll, c_int, Structure, c_long, POINTER, byref

import os

clib = cdll.LoadLibrary("libc.so.6")

CLOCK_MONOTONIC = 1
TFD_CLOEXEC = 0o02000000

timerfd_create = getattr(clib, "timerfd_create")
timerfd_create.restype = c_int
timerfd_create.argtypes = (c_int, c_int)

time_t = c_long

class timespec(Structure):
    _fields_ = [("tv_sec", time_t), ("tv_nsec", c_long)]

class itimerspec(Structure):
    _fields_ = [("it_interval", timespec), ("it_value", timespec)]


timerfd_settime = getattr(clib, "timerfd_settime")
timerfd_settime.restype = c_int
timerfd_settime.argtypes = (c_int, c_int, POINTER(itimerspec), POINTER(itimerspec))


class timerfd_t(object):
    def __init__(self, sf):
        self.sf = sf
        self._fd = None

    def start(self):
        assert self._fd is None, "Timer already started."
        self._fd = self._create_timerfd_of(self.seconds, self.nanoseconds)

    def reset(self):
        if self._fd is not None:
            self._drain_timerfd(self._fd)

    def stop(self):
        self.reset()
        os.close(self._fd)
        self._fd = None

    def fileno(self):
        return self._fd

    @property
    def seconds(self):
        return int(self.sf)

    @seconds.setter
    def seconds(self, sf):
        self.sf = sf

    @property
    def nanoseconds(self):
        return int(self.sf * 1e+9) - int(self.seconds * 1e+9)

    @staticmethod
    def _create_timerfd_of(seconds, nanoseconds=0):
        fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)
        assert fd > 0, "Failed to create timer fd"
        t = itimerspec()
        t.it_value.tv_sec = seconds
        t.it_value.tv_nsec = nanoseconds
        t.it_interval = t.it_value
        e = timerfd_settime(fd, 0, byref(t), None)
        assert e == 0, "Failed to set timer fd"
        return fd

    @staticmethod
    def _drain_timerfd(timer_fd):
        os.read(timer_fd, 8) # Drain timer FD


def main(args):
    """
    Shush, this is instead of having a WHOLE new file to just test this
    file.
    """
    import select, time

    DEFAULT_TARGET_S = 2.1
    DEFAULT_LOOP     = 3

    target_s = DEFAULT_TARGET_S
    num_loops = DEFAULT_LOOP

    argc = len(args)
    if argc > 1:
        try:
            target_s = float(args[1])
        except ValueError:
            pass
    if argc > 2:
        try:
            num_loops = int(args[2])
        except ValueError:
            pass

    timer = timerfd_t(target_s)
    start_time = time.monotonic()
    timer.start()

    r = select.select([timer], [], [], target_s * 2)
    if not r[0]:
        print("Hit timeout")
        return -1
    timer.reset()

    diff_time = time.monotonic() - start_time
    print(f"Target = {target_s}")
    print(f"Real = {diff_time}")

    start_time = time.monotonic()
    count = 0
    while count < num_loops:
        r = select.select([timer], [], [], target_s * 2)
        if not r[0]:
            print("Hit timeout")
            return -1
        timer.reset()
        count += 1
    diff_time = time.monotonic() - start_time
    print(f"Target = {target_s * num_loops}")
    print(f"Real = {diff_time}")

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
