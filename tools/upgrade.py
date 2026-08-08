#! /usr/bin/python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.path.pardir, "python"))

from binding import dev_t

dev = dev_t(sys.argv[1])
if "DEBUG" in os.environ:
    dev.set_dbg("0x800")
dev.fw_upload(sys.argv[2])
dev.reset()
