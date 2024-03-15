#!/usr/bin/env python3

import sys
import logging
import time
import socket
import subprocess
import random
import string
import os
import signal

from aiohttp import web
import asyncio

event_loop = asyncio.get_event_loop()
tcpsockets = []
all_processes = {}

PATH = os.path.dirname(os.path.abspath(__file__))

_RESPONSE_BEGIN = b"============{"
_RESPONSE_END = b"}============"

import atexit

def exit_handler():
    logging.critical('My application is ending!')
    for tcp in tcpsockets:
        logging.critical(tcp.close())

atexit.register(exit_handler)


class virtual_osm:
    def __init__(self, port, logger):
        self.port = port
        self._logger = logger

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
            self._logger.debug("Virtual OSM could not be generated.")

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
    def __init__(self, port, host, logger):
        self.port = port
        self.host = host
        self._logger = logger

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
        now = time.monotonic()
        end_time = now + timeout
        new_msg = None
        msg_str = b""
        msgs = []
        while now < end_time:
            new_msg = self.osm.recv(1024)
            if new_msg is None:
                self._logger.debug("NULL line read, probably closed")
                break
            new_msg = new_msg.strip(b"\n\r")
            msgs += [new_msg.decode()]
            msg_str += new_msg
            if _RESPONSE_END in msg_str:
                break
            now = time.monotonic()
        return msgs

    def do_cmd_multi(self, cmd):
        self.write(cmd)
        ret = self.readlines()
        start_pos = None
        for n in range(0, len(ret)):
            line = ret[n]
            if line == _RESPONSE_BEGIN:
                start_pos = n+1
        if start_pos is not None:
            return ret[start_pos:]
        return ret

    def do_cmd(self, cmd):
        r = self.do_cmd_multi(cmd)
        if r is None:
            return ""
        return "".join([line for line in r])

    def close(self):
        self.osm.close()
        self._logger.debug("TCP socket closed.")

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
                        web.get('/websocket', self.spawn_osm)
                        ])
        app.router.add_static('/', path=('.'),
                      name='app', show_index=True)
        runner = web.AppRunner(app)
        event_loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, port=self.port)
        event_loop.run_until_complete(site.start())
        event_loop.run_forever()


    async def spawn_osm(self, request):
        port = self.gen_random_port()

        vosm = virtual_osm(port, self._logger)
        osm = vosm.gen_virtual_osm_instance()

        ws = web.WebSocketResponse()
        await ws.prepare(request)

        tcp_client = osm_tcp_client(osm, self.host, self._logger)
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
                    output = tcp_client.do_cmd(msg.data)
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
        status = os.wait()
        self._logger.debug(f"Process terminated with pid {status[0]}")
        return status

    async def get_index(self, request):
        return web.FileResponse('./index.html')

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
