#! /usr/bin/python3

import time
import subprocess
import random
import string
import weakref
import socket
import select
import signal
import os
import logging
import sys

import aiohttp
from aiohttp import web

import asyncio
import nest_asyncio
nest_asyncio.apply()

PATH = os.path.dirname(os.path.abspath(__file__))

all_processes = {}


class osm_websocket_server:
    def __init__(self,tcpport, loc):
        self.tcpport = tcpport
        self.loc = loc

    async def start_websocket_osm(self):
        ws_port = 8765
        await self.open_tcp()
        loop = asyncio.get_event_loop()
        websocket_server = web.Application()
        websocket_server.add_routes([web.get(f'/api/{self.loc}', self.websocket_handler),
                                     web.post(f'/api/{self.loc}', self.websocket_post)])
        print(f"Running websocket on ws://0.0.0.0:{ws_port}/api/{self.loc}")
        runner = web.AppRunner(websocket_server)
        loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, port=ws_port)
        loop.run_until_complete(site.start())

    async def websocket_post(self, response):
        print(response)

    async def websocket_handler(self, request):
        print("new websocket")
        ws = web.WebSocketResponse()
        await ws.prepare(request)

        for msg in ws:
            print(ws)
            if msg.type == aiohttp.WSMsgType.TEXT:
                if msg.data == 'close':
                    ws.close()
                else:
                    ws.send_str(msg.data)
            elif msg.type == aiohttp.WSMsgType.ERROR:
                print('ws connection closed with exception %s' %
                    ws.exception())

        print('websocket connection closed')
        return ws

    async def open_tcp(self):
        self.tcp = osm_tcp_socket_client(self.tcpport, 'localhost')
        self.tcp.open_socket_connection()

    async def do_cmd(self, msg):
        await self.tcp.write(msg)
        output = await self.tcp.read()
        return output

class osm_tcp_socket_client:
    def __init__(self, port, host):
        self.port = port
        self.host = host

    def open_socket_connection(self):
        self.osm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.osm.connect((self.host, self.port))
        return self.osm

    def write(self, msg):
        self.osm.sendall((f"{msg}\n").encode())

    def readlines(self):
        now = time.monotonic()
        end_time = now + 1
        new_msg = None
        msgs = []
        end_line = "}============"
        while now < end_time:
            r = select.select([self.osm], [], [], end_time - now)
            if not r[0]:
                print("Lines timeout")
            new_msg = self.osm.recv(1024).decode()
            if new_msg is None:
                print("NULL line read.")
            if new_msg:
                new_msg = new_msg.strip("\n\r")
            if new_msg != end_line:
                msgs += [new_msg]
            else:
                break
            now = time.monotonic()
        return msgs

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

    def close(self):
        self.osm.close()

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


class close_handler(base_handler_t):
    def __init__(self, parent):
        super().__init__(parent)

    def do_POST(self, received_data: dict) -> tuple:
        p = received_data["port"]
        process = all_processes[p]
        fw_pid = int(process.pid)
        bash_pid = fw_pid + 1
        os.kill(bash_pid, signal.SIGINT)
        os.kill(fw_pid, signal.SIGINT)
        response = {'Closed': True}
        return web.json_response(response)



class spawn_virtual_osm_handler(base_handler_t):
    def __init__(self, parent):
        super().__init__(parent)

    def do_POST(self, received_data):
        self.port = self.gen_random_port()
        loc = self.random_str()
        location = f"/tmp/osm_{loc}/"
        url = f"ws://localhost:8765/api/{loc}"
        host = f"ws://localhost/api/{loc}"
        linux_osm = 0

        osm_bin_exists = self.find_linux_binary()
        if not osm_bin_exists:
            linux_osm = self.generate_linux_binary()

        response = {}

        if linux_osm == 0:
            cmd = [f"DEBUG=1 USE_PORT={self.port} LOC={location} ../../../build/penguin/firmware.elf"]
            response = {"location": location, "url": url, "port": self.port}
            self.spawn_virtual_osm(cmd, self.port)
            time.sleep(2)
            svr = osm_websocket_server(self.port, loc)
            asyncio.run(svr.start_websocket_osm())

        return web.json_response(response)

    @staticmethod
    def gen_random_port():
        port = random.randint(1096, 65535)
        return port

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

class master_request_handler:
    def __init__(self, is_verbose, logger=None):
        if logger is None:
            logger = logging.getLogger(__name__)
        self._logger = logger
        if is_verbose:
            logging.basicConfig(level=logging.DEBUG)
        self._handlers = \
            {
                "Close"                     : close_handler,
                "Spawn virtual OSM"         : spawn_virtual_osm_handler,
            }

    def run_server(self, port):
        self.app = web.Application()
        self.app.add_routes([web.get('/', self.do_get),
                        web.post('/', self.do_post)])
        self.app.router.add_static('/', path=('.'),
                      name='app')

        loop = asyncio.get_event_loop()
        runner = web.AppRunner(self.app)
        loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, port=port)
        loop.run_until_complete(site.start())
        loop.run_forever()


    async def do_get(self, request):
        return web.FileResponse('./index.html')

    async def do_post(self, request):
        received_data = await request.json()
        cmd = received_data.get("cmd")

        self._logger.debug("RECIEVED POST")
        cmd = received_data.get("cmd")
        if cmd is None:
            self._logger.error(f"NO COMMAND: {received_data}")
            return ret
        self._logger.debug(f"GOT CMD: '{cmd}'")
        handler_t = self._handlers.get(cmd)
        if handler_t is None:
            self._logger.error(f"NO HANDLER: {cmd}")
            return ret
        self._logger.debug(f"USING HANDLER TYPE: {handler_t}")
        try:
            ret = handler_t(self).do_POST(received_data)
        except NotImplementedError:
            ret = {}

        return ret

def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description="OSM Config GUI Server")
        parser.add_argument("-p", "--port"      , help="Serial Port"                    , required=False, type=int                  , default=8000          )
        parser.add_argument("-v", "--verbose"   , help="Verbose messages"               , action="store_true"                                               )
        return parser.parse_args()

    args = get_args()
    svr = master_request_handler(args.verbose)
    svr.run_server(args.port)

    return 0

if __name__ == "__main__":
    sys.exit(main())
