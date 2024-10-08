#!/usr/bin/env python3


import sys
import os
import select
import logging
import traceback
import serial
import subprocess


def _create_modbus_pty(name):
    master, slave = os.openpty()
    slave_path = os.readlink(f"/proc/self/fd/{slave}")
    port = f"/tmp/osm/{name}"
    if not os.path.exists(os.path.dirname(port)):
        os.mkdir(os.path.dirname(port))
    if os.path.exists(port):
        os.unlink(port)
    os.symlink(slave_path, port)
    logging.info(f"OPENED {name} (fd:{slave})")
    return master

def _clean_modbus_pty(name):
    port = f"/tmp/osm/{name}"
    if os.path.exists(port):
        os.unlink(port)
        logging.info(f"REMOVED {name}")

def _pty_connected(name):
    pids = subprocess.check_output(["fuser",f"/tmp/osm/{name}"], stderr=subprocess.DEVNULL)
    pids = pids.strip().split(b" ")
    pids = [int(p) for p in pids]
    pids.pop(pids.index(os.getpid()))
    return len(pids)

def main(args):

    MODBUS_MASTER = "MODBUS_MASTER"
    MODBUS_SLAVE = "MODBUS_SLAVE"

    FORMAT = ('%(asctime)-15s %(threadName)-15s'
              ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')
    logging.basicConfig(format=FORMAT)
    _log = logging.getLogger()

    if "DEBUG" in os.environ:
        _log.setLevel(logging.DEBUG)
    else:
        _log.setLevel(logging.CRITICAL)

    if len(args) > 1:
        dev = args[1]
    else:
        osm_loc = os.getenv("OSM_LOC", "/tmp/osm/")
        dev = os.path.join(osm_loc, "UART_EXT_slave")
        if not os.path.exists(osm_loc):
            os.mkdir(osm_loc)

    map_ = {}
    modbus_master_fd = _create_modbus_pty(MODBUS_MASTER)
    modbus_slave_fd = _create_modbus_pty(MODBUS_SLAVE)
    modbus_bridge = serial.Serial(dev)
    modbus_bridge_fd = modbus_bridge.fileno()

    logging.info(f"USING {dev} (fd:{modbus_bridge_fd})")

    def _modbus_master_on_read():
        c = os.read(fd, 1)
        if _pty_connected(MODBUS_MASTER):
            os.write(modbus_slave_fd, c)
            modbus_bridge.write(c)
            return c
        return b""

    def _modbus_slave_on_read():
        c = os.read(fd, 1)
        os.write(modbus_master_fd, c)
        modbus_bridge.write(c)
        return c

    def _modbus_bridge_on_read():
        if not _pty_connected(MODBUS_MASTER):
            c = modbus_bridge.read(1)
            os.write(modbus_slave_fd, c)
            return c
        return b""

    map_ = {
        modbus_master_fd    : (_modbus_master_on_read   , MODBUS_MASTER   ),
        modbus_slave_fd     : (_modbus_slave_on_read    , MODBUS_SLAVE    ),
        modbus_bridge    : (_modbus_bridge_on_read   , "MODBUS_BRIDGE"   ),
    }

    done = False
    while not done:
        try:
            r,w,x = select.select(map_.keys(), [], [], 0.01)
            for fd in r:
                cb,name = map_.get(fd, (None,None))
                if cb:
                    c = cb()
                    if c:
                        logging.debug(f"{name if name else fd} : {c}")
                else:
                    c = os.read(fd, 1)
                    logging.warning(f"Read for unused fd:{fd} : {c}")
        except KeyboardInterrupt:
            done = True
            logging.info("Caught keyboard interrupt")
        except Exception as e:
            lines = traceback.format_exception(e)
            for line in lines:
                logging.error(line)
            done = True

    _clean_modbus_pty(MODBUS_SLAVE)
    _clean_modbus_pty(MODBUS_MASTER)
    os.close(modbus_bridge_fd)
    os.close(modbus_slave_fd)
    os.close(modbus_master_fd)
    logging.debug("Finished cleanup")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
