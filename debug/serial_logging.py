#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  serial_logging.py
#  
#  Copyright 2022 Harry Geyer <hgeyer@Devtank>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  

import os
import sys
import select
import serial
import datetime


sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))


from python import dev_t


def log_out(file_, message, flush=False):
    time_str = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%S.%fZ")
    file_.write(f"{time_str}:{message}\n")
    if flush:
        file_.flush()



def main(args):

    dev = dev_t(args[1])
    f = open(args[2], "w")

    last_msg = ""
    while True:
        r = select.select([dev],[],[], 10)
        if len(r[0]):
            msgs = dev._ll.readlines()
            for msg in msgs:
                log_out(f, msg, True)
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
