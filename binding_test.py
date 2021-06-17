#! /usr/bin/python3
import binding
import serial
import sys

if len(sys.argv) < 2:
    print("Requires UART")
    exit(-1)
else:
    devpath = sys.argv[1]

comm = binding.io_board_py_t(devpath)

for pps in comm.ppss:
    print("PPS", pps.index, pps.values)

for adc in comm.adcs:
    print("ADC", adc.index, adc.values, adc.unit)

for io in comm.ios:
    print("IO %u label:%s is_locked:%s has_special:%s is_special:%s dir:%s bias:%s value:%s" % (io.index, io.label, io.is_locked, io.has_special, io.special, io.direction, io.bias, io.value))

for uart in comm.uarts:
    print("UART", uart.index, uart.baud, uart.bytesize, uart.parity, uart.stopbits)

comm.enable_calibdation = True
assert comm.enable_calibdation

comm.adc_rate_ms = 500
assert comm.adc_rate_ms == 500

comm.sync_all_adcs()
for adc in comm.adcs:
    print("ADC", adc.index, adc.values, adc.unit)

print("Get calibration")
d = comm.adcs_calibration
print("Set calibration")
comm.adcs_calibration = d
