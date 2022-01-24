#! /usr/bin/python3

import sys

from binding import dev_t

dev = dev_t(sys.argv[1])

_ANSI_RED     = "\x1B[31m"
_ANSI_GREEN   = "\x1B[32m"
_ANSI_DEFAULT = "\x1B[39m"

def threshold_check(value, ref, tolerance, unit, desc):
    fail = abs(value - ref) > tolerance
    print("%s%s : %G%s is %G%s +/- %G = %s%s" % (_ANSI_RED if fail else _ANSI_GREEN, desc, value, unit, ref, unit, tolerance, "FAIL" if fail else "Pass", _ANSI_DEFAULT))

def output_normal(msg):
    print(msg)


# ---------------------------


threshold_check(dev.w1.value, 17, 5, "degreeC", "OneWire Temp")

pm25, pm10 = dev.hpm.value

threshold_check(pm10, 5, 5, "", "PM10")
threshold_check(pm25, 7, 5, "", "PM25")


