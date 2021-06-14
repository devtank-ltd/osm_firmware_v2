#! /usr/bin/python3
import binding
import serial
import yaml
import sys
import os

if len(sys.argv) < 2:
    print("Requires UART", file=sys.stderr)
    exit(-1)
else:
    devpath = sys.argv[1]

comm = binding.io_board_py_t(devpath)

if os.isatty(0):
    d = comm.adcs_calibration

    yaml.dump(d, sys.stdout)
else:
    d = yaml.safe_load(sys.stdin)

    comm.adcs_calibration = d
