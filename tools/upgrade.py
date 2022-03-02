#! /usr/bin/python3
import os
import sys
from binding import io_board_py_t

dev = io_board_py_t(sys.argv[1])
if "DEBUG" in os.environ:
    dev.command(b"debug 0x800")
dev.fw_upload(sys.argv[2])
dev.reset()
