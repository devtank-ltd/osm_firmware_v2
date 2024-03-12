#!/usr/bin/env python3

import sys
import logging
import time
import socket
import select
import subprocess
import random
import string
import os
import signal
import base64
import json
import weakref

import aiohttp
from aiohttp import web
import asyncio
from websockets.server import serve

event_loop = asyncio.get_event_loop()
tcpsockets = []
all_processes = {}

PATH = os.path.dirname(os.path.abspath(__file__))

import atexit

def exit_handler():
    logging.critical('My application is ending!')
    for tcp in tcpsockets:
        logging.critical(tcp.close())

atexit.register(exit_handler)

class base_handler_t(object):
    def __init__(self, parent):
        self._parent = parent
        self._logger = self.parent._logger

    @property
    def parent(self):
        return weakref.ref(self._parent)()

    def do_GET(self, received_data: dict) -> tuple:
        raise NotImplementedError

    def do_POST(self, received_data: dict) -> tuple:
        raise NotImplementedError

class latest_fw_version(base_handler_t):
    LATEST_VERSION_INFO_PATH = "version_info.txt"

    def _do_cmd(self, cmd: str) -> str:
        return subprocess.check_output(cmd, shell=True)

    def _read_info(self):
        with open(self.LATEST_VERSION_INFO_PATH, "r") as f:
            info = f.read()
        self._logger.debug(f"READ INFO: {info}")
        return json.loads(info)

    def do_GET(self):
        self._logger.debug("SENDING VERSION")
        return self._read_info()

class latest_fw_file(base_handler_t):
    LATEST_FIRMWARE_PATH = "fw_releases/complete.bin"

    def _read_firmware(self):
        with open(self.LATEST_FIRMWARE_PATH, "rb") as f:
            bin_data = f.read()
        return bin_data

    def do_GET(self):
        self._logger.debug("SENDING BINARY FILE")
        firmware_bin = self._read_firmware()
        firmware_b64 = base64.b64encode(firmware_bin)
        return firmware_b64

class virtual_osm:
    def __init__(self, port):
        self.port = port

    def gen_virtual_osm_instance(self):
        rstr = self._random_str()
        self.loc = f"/tmp/osm_{rstr}/"

        linux_osm = 0

        osm_bin_exists = self._find_linux_binary()
        if not osm_bin_exists:
            linux_osm = self._generate_linux_binary()

        if linux_osm == 0:
            cmd = [f"DEBUG=1 LOC={self.loc} USE_PORT={self.port} ../../../build/penguin/firmware.elf"]
            self._spawn_virtual_osm(cmd, self.port)
            time.sleep(2)
        else:
            print("Virtual OSM could not be generated.")

        return self.port

    @staticmethod
    def _random_str():
        length = 20
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(length))

    def _spawn_virtual_osm(self, cmd, port):
        self.vosm_subp = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        all_processes[port] = self.vosm_subp
        return self.vosm_subp

    @staticmethod
    def _find_linux_binary():
        output = f"{PATH}/../../../build/penguin/firmware.elf"
        try:
            sub = subprocess.check_output(f"ls {output}", shell=True)
            return True
        except:
            return False

    @staticmethod
    def _generate_linux_binary():
        sub = subprocess.run(f"cd {PATH}/../../.. && make penguin && cd -", stdout=subprocess.PIPE, shell=True)
        return sub.returncode


class osm_tcp_client:
    def __init__(self, port, host):
        self.port = port
        self.host = host

    def osm_svr(self):
        self.osm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.osm.connect((self.host, self.port))
        except ConnectionRefusedError:
            return False
        return self.osm

    def write(self, msg):
        self.osm.sendall((f"{msg}\n").encode())

    def readlines(self, timeout=1):
        end_line = "}========"
        now = time.monotonic()
        end_time = now + timeout
        new_msg = None
        msgs = b""
        while now < end_time:
            new_msg = self.osm.recv(1024)
            if new_msg is None:
                print("NULL line read.")
            if new_msg != end_line.encode():
                msgs += new_msg
            if end_line.encode() in msgs:
                break
            now = time.monotonic()
        return msgs.decode()

    def read(self):
        now = time.monotonic()
        end_time = now + 1
        new_msg = None
        while now < end_time:
            r = select.select([self.osm], [], [], end_time - now)
            if not r[0]:
                print("Lines timeout")
            new_msg = self.osm.recv(1024).decode()
            return new_msg

    async def do_cmd(self, msg):
        self.write(msg)
        ret = self.readlines()
        return ret

    def close(self):
        self.osm.close()
        print("TCP socket closed.")

class http_server:
    def __init__(self, port, is_verbose, host, logger=None):
        self.port = port
        self.host = host
        if logger is None:
            logger = logging.getLogger(__name__)
            self._logger = logger
            if is_verbose:
                logging.basicConfig(level=logging.DEBUG)
        self._logger.info(f"Running on {self.host} on port {self.port}")

    @staticmethod
    def gen_random_port():
        port = random.randint(1096, 65535)
        return port

    def run_server(self):
        app = web.Application()
        app.add_routes([web.get('/', self.get_index),
                        web.get('/api', self.spawn_osm),
                        web.get('/latest_firmware_info', self.do_json_GET),
                        web.get('/latest_firmware', self.do_bin_GET)
                        ])
        app.router.add_static('/', path=('.'),
                      name='app', show_index=True)
        self._get_handlers = \
            {
                "/latest_firmware_info"     : latest_fw_version,
                "/latest_firmware"          : latest_fw_file,
            }
        runner = web.AppRunner(app)
        event_loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, port=self.port)
        event_loop.run_until_complete(site.start())
        event_loop.run_forever()


    async def spawn_osm(self, request):
        port = self.gen_random_port()

        vosm = virtual_osm(port)
        osm = vosm.gen_virtual_osm_instance()

        ws = web.WebSocketResponse()
        await ws.prepare(request)

        tcp_client = osm_tcp_client(osm, self.host)
        svr = tcp_client.osm_svr()
        if not svr:
            return False
        tcpsockets.append(svr)

        async for msg in ws:
            if msg.type == web.WSMsgType.CLOSE:
                self._logger.debug("It's over!")
                break
            elif msg.type == web.WSMsgType.TEXT:
                if msg.data == "close\n":
                    break
                else:
                    output = await tcp_client.do_cmd(msg.data)
                    await ws.send_str(output)
            elif msg.type == web.WSMsgType.ERROR:
                self.close_socket(port, tcp_client)
                self._logger.debug('ws connection closed with exception %s' %
                    ws.exception())

        self.close_socket(port, tcp_client)
        await ws.close()
        return ws

    def close_socket(self, port, client):
        client.close()
        process = all_processes[port]
        fw_pid = int(process.pid)
        bash_pid = fw_pid + 1
        os.kill(bash_pid, signal.SIGINT)
        os.kill(fw_pid, signal.SIGINT)

    async def get_index(self, request):
        return web.FileResponse('./index.html')

    def do_json_GET(self, request):
        handler = self._get_handlers.get("/latest_firmware_info")
        if handler is not None:
            ret = handler(self).do_GET()
            self._logger.debug(f"SENT: '{ret}'")
            return web.json_response(ret)

    def do_bin_GET(self, request):
        handler = self._get_handlers.get("/latest_firmware")
        if handler is not None:
            ret = handler(self).do_GET()
            self._logger.debug(f"SENT BIN DATA: {ret}: SIZ:{len(ret)}")
            return web.Response(body=ret, content_type='bin')

def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description="OSM Config GUI Server")
        parser.add_argument("-p", "--port"      , help="Serial Port"                    , required=False, type=int                  , default=8000          )
        parser.add_argument("-v", "--verbose"   , help="Verbose messages"               , action="store_true"                                               )
        parser.add_argument("-H", "--host"      , help="Host url"                       , type=str                                  , default='127.0.0.1'   )

        return parser.parse_args()

    args = get_args()
    svr = http_server(args.port, args.verbose, args.host)
    svr.run_server()

    return 0

if __name__ == "__main__":
    sys.exit(main())
