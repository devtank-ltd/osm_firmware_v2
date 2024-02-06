#!/usr/bin/env python3

import http.server
import socketserver
import json
import random
from threading import Thread
import signal
import time
import os
import subprocess
import string
import atexit

all_processes = []

class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.path = './config_gui.html'
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        self.send_response(200)

        self.send_header('Content-type', 'application/json')
        self.end_headers()

        port = self.gen_random_port()
        loc = self.random_str()
        location = f"/tmp/osm_{loc}/"
        websocket = f"ws://localhost:{port}/websocket"

        cmd = f"DEBUG=1 USE_PORT={port} USE_WS=1 LOC={location} ../../../build/penguin/firmware.elf"

        response = json.dumps({'location': location, "websocket": websocket})
        self.wfile.write(response.encode('utf-8'))

        self.spawn_virtual_osm(cmd)
        return response

    def gen_random_port(self):
        port = random.randint(1096, 65535)
        return port

    def random_str(self):
        length = 20
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(length))

    def spawn_virtual_osm(self, cmd):
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        all_processes.append(p)


@atexit.register
def cleanup():
    timeout_sec = 5
    for p in all_processes:
        p_sec = 0
        for second in range(timeout_sec):
            if p.poll() == None:
                time.sleep(1)
                p_sec += 1
        if p_sec >= timeout_sec:
            p.kill()
            global stop_threads
            stop_threads = True
            t.join()


def run_forever():
    handler_object = MyHttpRequestHandler
    PORT = 8000
    my_server = socketserver.TCPServer(("", PORT), handler_object)
    while True:
        my_server.serve_forever()
        if stop_threads:
            break

stop_threads = False
t = Thread(target=run_forever)
t.run()
