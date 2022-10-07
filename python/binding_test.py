#! /usr/bin/python3

import sys

from binding import dev_t

dev = dev_t(sys.argv[1])

_ANSI_RED     = "\x1B[31m"
_ANSI_GREEN   = "\x1B[32m"
_ANSI_DEFAULT = "\x1B[39m"

def threshold_check(value, ref, tolerance, unit, desc):
    print(value)
    fail = abs(value - ref) > tolerance
    print("%s%s : %G%s is %G%s +/- %G = %s%s" % (_ANSI_RED if fail else _ANSI_GREEN, desc, value, unit, ref, unit, tolerance, "FAIL" if fail else "Pass", _ANSI_DEFAULT))

def test_check(bool, desc):
    if bool == False:
        result = "LoRaWAN FAIL"
    else:
        result = "LoRaWAN PASS"
    print("%s - %s" % (bool, result))

def output_normal(msg):
    print(msg)


# ---------------------------

w1 = dev.extract_val("w1 TMP2", "TMP2")
threshold_check(w1, 17, 5, "degreeC", "OneWire Temp")
#store_value("OneWire Temp", dev.w1.value)

pm10 = dev.extract_val("hpm", "PM10")
threshold_check(pm10, 5, 5, "", "PM10")

pm25 = dev.extract_val("hpm", "PM25")
threshold_check(pm25, 7, 5, "", "PM25")

cc = dev.extract_val("cc", "Current Clamp")
threshold_check(cc, 7, 1, "amps", "Current Clamp")

lora = dev.extract_val("lora_conn", "LoRaWan")
test_check(lora, "loRaWAN connection established")

PF = dev.get_modbus_val("PF")
threshold_check(PF, 1, 0.2, "", "Power Factor")

CVP1 = dev.get_modbus_val("cVP1")
threshold_check(CVP1, 24000, 10, "volts", "P1 Voltage")

CVP2 = dev.get_modbus_val("cVP2")
threshold_check(CVP2, 24000, 10, "volts", "P2 Voltage")

CVP3 = dev.get_modbus_val("cVP3")
threshold_check(CVP3, 24000, 10, "volts", "P3 Voltage")

MAP1 = dev.get_modbus_val("mAP1")
threshold_check(MAP1, 3000, 200, "milliamps", "P1 milliamps")

MAP2 = dev.get_modbus_val("mAP2")
threshold_check(MAP2, 3000, 200, "milliamps", "P2 milliamps")

MAP3 = dev.get_modbus_val("mAP3")
threshold_check(MAP3, 3000, 200, "milliamps", "P3 milliamps")

ImpEnergy = dev.get_modbus_val("ImEn")
threshold_check(ImpEnergy, 1000, 100, "", "Power consumption")

