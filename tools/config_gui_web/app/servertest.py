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

class virtual_osm:
    def __init__(self, port):
        self.port = port

    def generate_osm(self):
        rstr = self.random_str()
        self.loc = f"/tmp/osm_{rstr}/"

        linux_osm = 0

        osm_bin_exists = self.find_linux_binary()
        if not osm_bin_exists:
            linux_osm = self.generate_linux_binary()

        if linux_osm == 0:
            cmd = [f"DEBUG=1 LOC={self.loc} USE_PORT={self.port} ../../../build/penguin/firmware.elf"]
            self.spawn_virtual_osm(cmd, self.port)
            time.sleep(2)

        return self.port

    @staticmethod
    def random_str():
        length = 20
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(length))

    def spawn_virtual_osm(self, cmd, port):
        self.vosm_subp = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        all_processes[port] = self.vosm_subp
        return self.vosm_subp

    @staticmethod
    def find_linux_binary():
        output = f"{PATH}/../../../build/penguin/firmware.elf"
        try:
            sub = subprocess.check_output(f"ls {output}", shell=True)
            return True
        except:
            return False

    @staticmethod
    def generate_linux_binary():
        sub = subprocess.run(f"cd {PATH}/../../.. && make penguin && cd -", stdout=subprocess.PIPE, shell=True)
        return sub.returncode


class osm_tcp_client:
    def __init__(self, port, host):
        self.port = port
        self.host = host

    def osm_svr(self):
        self.osm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.osm.connect((self.host, self.port))
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

    async def read_raw(self):
        new_msg = self.osm.recv(1024).decode()
        return new_msg

    async def do_cmd(self, msg):
        self.write(msg)
        ret = self.readlines()
        return ret

    def close(self):
        self.osm.close()
        print("CLOSED")

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
        app.add_routes([web.get('/', self.do_get),
                        web.get('/api', self.spawn_osm)])
        app.router.add_static('/', path=('.'),
                      name='app', show_index=True)
        runner = web.AppRunner(app)
        event_loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, port=self.port)
        event_loop.run_until_complete(site.start())
        event_loop.run_forever()

    def _tcp_socket_in(self):
        lines = self.tcp_client.readlines()
        for line in lines:
            self.ws.send_str(line)

    async def spawn_osm(self, request):
        self.port = self.gen_random_port()

        self.vosm = virtual_osm(self.port)
        osm = self.vosm.generate_osm()

        self.ws = web.WebSocketResponse()
        await self.ws.prepare(request)

        self.tcp_client = osm_tcp_client(osm, self.host)
        svr = self.tcp_client.osm_svr()
        tcpsockets.append(svr)

        async for msg in self.ws:
            if msg.type == web.WSMsgType.CLOSE:
                print("It's over!")
                break
            elif msg.type == web.WSMsgType.TEXT:
                if msg.data == "close\n":
                    break
                else:
                    output = await self.tcp_client.do_cmd(msg.data)
                    await self.ws.send_str(output)
            elif msg.type == web.WSMsgType.ERROR:
                self.close_socket(self.port)
                print('ws connection closed with exception %s' %
                    self.ws.exception())

        self.close_socket(self.port)
        print("Websocket connection closed")
        await self.ws.close()
        self.ws = None
        return self.ws

    def close_socket(self, port):
        self.tcp_client.close()
        print("TCP CLIENT CLOSED")
        process = all_processes[port]
        fw_pid = int(process.pid)
        bash_pid = fw_pid + 1
        os.kill(bash_pid, signal.SIGINT)
        os.kill(fw_pid, signal.SIGINT)

    async def do_get(self, request):
        return web.FileResponse('./index.html')

def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description="OSM Config GUI Server")
        parser.add_argument("-p", "--port"      , help="Serial Port"                    , required=False, type=int                  , default=8000          )
        parser.add_argument("-v", "--verbose"   , help="Verbose messages"               , action="store_true"                                               )
        parser.add_argument("-H", "--host"      , help="Host url"                       , type=str                                  , default='localhost'   )

        return parser.parse_args()

    args = get_args()
    svr = http_server(args.port, args.verbose, args.host)
    svr.run_server()

    return 0

if __name__ == "__main__":
    sys.exit(main())
