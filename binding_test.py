#! /usr/bin/python3
import binding
import serial
import sys

devpath = None

for p in serial.tools.list_ports.comports():
    if p.product == "STM32 STLink":
        devpath = p.device

if devpath:
    print("Found device :", devpath)
else:
    print("Not STM32 STLink found.")
    sys.exit(-1)

comm = binding.io_board_py_t(devpath)

for pps in comm.ppss:
    print("PPS", pps.index, pps.value)

for gpio_in in comm.inputs:
    print("Input", gpio_in.index, gpio_in.value)

for gpio_out in comm.outputs:
    print("Output", gpio_out.index, gpio_out.value)

for adc in comm.adcs:
    print("ADC", adc.index, adc.min_value, adc.max_value, adc.avg_value)
