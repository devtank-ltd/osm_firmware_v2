#! /usr/bin/python3
import binding
import serial
import sys

devpath = None

for p in serial.tools.list_ports.comports():
    if p.product == "IO Board":
        devpath = p.device

if devpath:
    print("Found device :", devpath)
else:
    print("Not STM32 STLink found.")
    sys.exit(-1)

comm = binding.io_board_py_t(devpath)

for pps in comm.ppss:
    print("PPS", pps.index, pps.values)

for adc in comm.adcs:
    print("ADC", adc.index, adc.values)

for gpio in comm.gpios:
    print("GPIO", gpio.index, gpio.info)

for uart in comm.uarts:
    print("UART", uart.index, uart.baud, uart.bytesize, uart.parity, uart.stopbits)
