#!/usr/bin/env python3

import http.server
from http import HTTPStatus
import socketserver
import json
import random
from threading import Thread
import signal
import time
import os
import sys
import subprocess
import string
import atexit
import logging
import weakref

PATH = os.path.dirname(os.path.abspath(__file__))

all_processes = {}
all_threads = []


class base_handler_t(object):
    def __init__(self, fparent, parent):
        self._parent = parent
        self._logger = self.parent._logger
        self._logger.critical(parent)

    @property
    def parent(self):
        return weakref.ref(self._parent)()

    def do_GET(self, received_data: dict) -> tuple:
        raise NotImplementedError

    def do_POST(self, received_data: dict) -> tuple:
        raise NotImplementedError


class close_handler(base_handler_t):
    def __init__(self, parent):
        super().__init__(self, parent)

    def do_POST(self, received_data: dict) -> tuple:
        p = received_data["port"]
        process = all_processes[p]
        fw_pid = int(process.pid)
        bash_pid = fw_pid + 1
        os.kill(bash_pid, signal.SIGINT)
        os.kill(fw_pid, signal.SIGINT)
        response = {'Closed': True}
        return (HTTPStatus.OK, response)


class spawn_virtual_osm_handler(base_handler_t):
    def __init__(self, parent):
        super().__init__(self, parent)

    def do_POST(self, received_data: dict) -> tuple:
        self.port = self.gen_random_port()
        loc = self.random_str()
        location = f"/tmp/osm_{loc}/"
        websocket = f"ws://localhost:{self.port}/websocket"
        linux_osm = 0
        fake_linux_exists = self.check_server()
        if not fake_linux_exists:
            response = json.dumps({"msg": "unavailable"})
            self.wfile.write(response.encode("utf-8"))
            return response

        osm_bin_exists = self.find_linux_binary()
        if not osm_bin_exists:
            linux_osm = self.generate_linux_binary()

        response = {}

        if linux_osm == 0:
            cmd = [f"DEBUG=1 USE_PORT={self.port} USE_WS=1 LOC={location} ../../../build/penguin/firmware.elf"]
            response = {'location': location, "websocket": websocket, "port": self.port}
            self.spawn_virtual_osm(cmd, self.port)
            time.sleep(2)

        return (HTTPStatus.OK, response)

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
        except FileNotFoundError:
            return False

    @staticmethod
    def generate_linux_binary():
        sub = subprocess.run(f"cd {PATH}/../../.. && make penguin && cd -", stdout=subprocess.PIPE, shell=True)
        return sub.returncode

    @staticmethod
    def check_server():
            cmd = f"hostname"
            host = subprocess.check_output(cmd, shell=True).decode()
            if host != "osm-config":
                return True
            else:
                return False

class latest_fw_version(base_handler_t):
    def do_POST(self):
        self._logger.debug("SENDING VERSION")
        return (HTTPStatus.OK, {"version": "new"})

class latest_fw_file(base_handler_t):
    def do_POST(self):
        pass

class master_request_handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, request, client_address, server, directory=None, logger=None):
        if logger is None:
            logger = logging.getLogger(__name__)
        self._logger = logger
        self._handlers = \
            {
                "Close"                     : close_handler,
                "Spawn virtual OSM"         : spawn_virtual_osm_handler,
                "Get latest fw version"     : latest_fw_version,
                "Get latest fw file"        : latest_fw_file,
            }
        self._request = request
        self._client_address = client_address
        self._server = server
        self._directory = directory
        super().__init__(request, client_address, server, directory=directory)

    def _read_content(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        received_data = json.loads(post_data.decode('utf-8'))
        return received_data

    def _send_response_json(self, response_code: HTTPStatus, response: dict):
        self.send_response(response_code.value)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        response_json = json.dumps(response)
        self.wfile.write(response_json.encode('utf-8'))
        self._logger.debug(f"SENT: {response_code.name} ({response_code.value}): '{response_json}'")

    def do_GET(self):
        if self.path == '/':
            self.path = './index.html'
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

    def do_POST(self):
        self._logger.debug("RECIEVED POST")
        received_data = self._read_content()
        cmd = received_data.get("cmd")
        if cmd is None:
            self._logger.error(f"NO COMMAND: {received_data}")
            response, ret = (HTTPStatus.NOT_FOUND, {})
            self._send_response_json(response, ret)
            return ret
        self._logger.debug(f"GOT CMD: '{cmd}'")
        handler_t = self._handlers.get(cmd)
        if handler_t is None:
            self._logger.error(f"NO HANDLER: {cmd}")
            response, ret = (HTTPStatus.NOT_FOUND, {})
            self._send_response_json(response, ret)
            return ret
        self._logger.debug(f"USING HANDLER TYPE: {handler_t}")
        try:
            response, ret = handler_t(self).do_POST(received_data)
        except NotImplementedError:
            response, ret = (HTTPStatus.NOT_FOUND, {})
        self._logger.info(f"RETURNING: {response.name} ({response.value}): '{ret}'")
        self._send_response_json(response, ret)
        return ret


def run_server(port, is_verbose):
    class server_t(object):
        def __init__(self, port=8000):
            self._done = False
            handler_object = master_request_handler
            socketserver.TCPServer.allow_reuse_address = True
            self._webserver = socketserver.TCPServer(("", port), handler_object)

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.cleanup()
            self._webserver.__exit__(exc_type, exc_val, exc_tb)

        def cleanup(self):
            timeout_sec = 5
            for key, p in all_processes.items():
                p_sec = 0
                for second in range(timeout_sec):
                    if p.poll() == None:
                        time.sleep(1)
                        p_sec += 1
                if p_sec >= timeout_sec:
                    p.kill()
                    global stop_threads
                    stop_threads = True
                    for thr in all_threads:
                        thr.join()

        def run_forever(self):
            while not self._done:
                self._webserver.serve_forever()
                if stop_threads:
                    break

    if is_verbose:
        logging.basicConfig(level=logging.DEBUG)

    with server_t(port=port) as server:
        server.run_forever()

def main():
    import argparse

    def get_args():
        parser = argparse.ArgumentParser(description="OSM Config GUI Server")
        parser.add_argument("-p", "--port"      , help="Serial Port"                    , required=False, type=int                  , default=8000          )
        parser.add_argument("-v", "--verbose"   , help="Verbose messages"               , action="store_true"                                               )
        return parser.parse_args()

    args = get_args()

    stop_threads = False
    t = Thread(target=run_server, args=(args.port, args.verbose))
    t.start()
    all_threads.append(t)
    return 0

if __name__ == "__main__":
    sys.exit(main())
