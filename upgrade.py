#! /usr/bin/python3

import sys
from binding import io_board_py_t

dev = io_board_py_t(sys.argv[1])
dev.fw_upload(sys.argv[2])
dev.reset()
